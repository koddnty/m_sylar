#include "basic/config.h"
#include "basic/fdManager.h"
#include "basic/log.h"
#include "basic/timer/timer.hpp"
#include "coroutine/corobase.h"

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

/**
    hook层fd事件超时机制测试:
        1. fd未设置SO_RCVTIMEO时, 使用默认值HOOK_IOAWAIT_TIMEOUT, 超时返回-1且errno=ETIMEDOUT
        2. fd设置了SO_RCVTIMEO时, 按该值超时, 且超时后事件监听的定时器不会二次唤醒协程
        3. 事件在超时前到达时, 正常读取数据, 不被超时逻辑破坏
        4. co_connect: 连接就绪不会被误判为超时(返回0), 连接失败不返回伪成功

    注意: 多线程下协程可能被调度到任意iomanager线程, 因此用例结果通过原子变量回传,
          errno在协程内读取(与被恢复的线程一致)。
*/

using namespace m_sylar;

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
static int g_failed = 0;

static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << what << std::endl;
    if(!ok) { ++g_failed; }
}

// 等待原子变量离开初值, 超时返回false
static bool waitValue(std::atomic<int>& v, int init, uint64_t timeout_ms)
{
    uint64_t begin = TimeManager::GetCurrentMS();
    while(v.load() == init)
    {
        if(TimeManager::GetCurrentMS() - begin > timeout_ms) { return false; }
        usleep(1000);
    }
    return true;
}

static bool inRange(uint64_t v, uint64_t lo, uint64_t hi)
{
    return v >= lo && v <= hi;
}

// 关闭测试fd: 必须先从FdMgr注销, 否则fd号被下次socketpair复用时
// FdMgr会返回陈旧的FdCtx, 新socket不会被FdCtx::init设置为非阻塞
static void closeFd(int fd)
{
    if(fd < 0) { return; }
    FdMgr::GetInstance()->del(fd);
    ::close(fd);
}

static void closeFds(int fds[2])
{
    closeFd(fds[0]);
    closeFd(fds[1]);
    fds[0] = fds[1] = -1;
}

// ------------------------- 读超时用例 -------------------------
struct ReadCase
{
    int fd = -1;
    int peer_fd = -1;
    uint64_t fd_timo_us = 0;            // 0: 不设置fd超时, 使用默认值
    int peer_write_delay_ms = -1;       // >=0: 延时后由对端写入数据
    int peer_write_len = 0;

    std::atomic<int> done {0};          // 协程结束标记
    std::atomic<int> resumes {0};       // 协程被恢复的次数, >1说明有残留的定时器/事件二次唤醒
    std::atomic<int> rt {0};
    std::atomic<int> errno_val {0};
    std::atomic<uint64_t> cost_ms {0};
};

static Task<void, TaskBeginExecuter> co_readCase(std::shared_ptr<ReadCase> c)
{
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(c->fd, true);     // 注册为受管理的socket fd
    if(!fd_ctx)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "register fd failed, fd = " << c->fd;
        c->rt = -2;
        c->done = 1;
        co_return;
    }
    if(c->fd_timo_us) { fd_ctx->setTimeout(SO_RCVTIMEO, c->fd_timo_us); }

    if(c->peer_write_delay_ms >= 0)
    {   // 由定时器在延时后向对端写入数据
        int peer = c->peer_fd;
        int len = c->peer_write_len;
        TimeTask::ptr task = TimeTask::create(c->peer_write_delay_ms, false,
            [peer, len](TimeTask::ptr) -> Task<void> {
                std::string data(len, 'x');
                ssize_t n = ::write(peer, data.data(), data.size());
                if(n != (ssize_t)data.size())
                {
                    M_SYLAR_LOG_ERROR(g_logger) << "peer write failed, n = " << n;
                }
                co_return;
            });
        TimeManager::getInstance()->addTimer(task);
    }

    char buf[64] = {0};
    uint64_t begin = TimeManager::GetCurrentMS();
    int rt = co_await co_read(c->fd, buf, sizeof(buf));
    c->cost_ms = TimeManager::GetCurrentMS() - begin;
    c->errno_val = errno;
    c->rt = rt;
    c->resumes++;
    c->done = 1;
    co_return;
}

// ------------------------- connect 用例 -------------------------
struct ConnectCase
{
    std::string ip = "127.0.0.1";
    uint16_t port = 0;

    std::atomic<int> done {0};
    std::atomic<int> rt {0};
    std::atomic<int> errno_val {0};
};

static Task<void, TaskBeginExecuter> co_connectCase(std::shared_ptr<ConnectCase> c)
{
    int fd = co_socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "create socket failed";
        c->rt = -2;
        c->done = 1;
        co_return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(c->port);
    inet_pton(AF_INET, c->ip.c_str(), &addr.sin_addr);

    int rt = co_await co_connect(fd, (sockaddr*)&addr, sizeof(addr));
    c->errno_val = errno;
    c->rt = rt;
    c->done = 1;
    co_close(fd);
    co_return;
}

// 创建一个监听本地端口的socket, 返回fd, 端口写入port_out
static int makeListenSocket(uint16_t* port_out)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) { return -1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if(::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0 || ::listen(fd, 8) < 0)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "listen failed, errno = " << errno;
        ::close(fd);
        return -1;
    }
    socklen_t len = sizeof(addr);
    if(::getsockname(fd, (sockaddr*)&addr, &len) < 0)
    {
        ::close(fd);
        return -1;
    }
    *port_out = ntohs(addr.sin_port);
    return fd;
}

// 取一个当前未被使用的本地端口
static uint16_t getFreePort()
{
    uint16_t port = 0;
    int fd = makeListenSocket(&port);
    if(fd >= 0) { ::close(fd); }
    return port;
}

// ------------------------- 用例 -------------------------

// 用例1: 未设置fd超时, 使用默认值
static void testDefaultTimeout(IOManager* iom)
{
    int fds[2] = {-1, -1};
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
    {
        check(false, "默认超时: socketpair 创建失败");
        return;
    }
    auto c = std::make_shared<ReadCase>();
    c->fd = fds[0];
    c->peer_fd = fds[1];
    iom->schedule(TaskCoro20::create_coro([c]() { return co_readCase(c); }));

    bool ok = waitValue(c->done, 0, HOOK_IOAWAIT_TIMEOUT + 5000);
    check(ok, "默认超时: 协程被唤醒(未挂死)");
    if(ok)
    {
        check(c->rt == -1, "默认超时: 返回-1, 实际=" + std::to_string(c->rt.load()));
        check(c->errno_val == ETIMEDOUT,
              "默认超时: errno=ETIMEDOUT, 实际=" + std::to_string(c->errno_val.load()));
        check(inRange(c->cost_ms, HOOK_IOAWAIT_TIMEOUT - 200, HOOK_IOAWAIT_TIMEOUT + 2000),
              "默认超时: 耗时≈" + std::to_string(HOOK_IOAWAIT_TIMEOUT) + "ms, 实际="
              + std::to_string(c->cost_ms.load()) + "ms");
    }
    closeFds(fds);
}

// 用例2: 设置fd超时(200ms), 且超时后不再有残留唤醒
static void testFdTimeout(IOManager* iom)
{
    int fds[2] = {-1, -1};
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
    {
        check(false, "fd超时: socketpair 创建失败");
        return;
    }
    auto c = std::make_shared<ReadCase>();
    c->fd = fds[0];
    c->peer_fd = fds[1];
    c->fd_timo_us = 200 * 1000;         // 200ms
    iom->schedule(TaskCoro20::create_coro([c]() { return co_readCase(c); }));

    bool ok = waitValue(c->done, 0, 3000);
    check(ok, "fd超时: 协程被唤醒");
    if(ok)
    {
        check(c->rt == -1, "fd超时: 返回-1, 实际=" + std::to_string(c->rt.load()));
        check(c->errno_val == ETIMEDOUT,
              "fd超时: errno=ETIMEDOUT, 实际=" + std::to_string(c->errno_val.load()));
        check(inRange(c->cost_ms, 150, 1000),
              "fd超时: 耗时≈200ms, 实际=" + std::to_string(c->cost_ms.load()) + "ms");
    }
    // 超时后addEventWithTimeout已取消事件监听, 等待数倍超时时间不应再被唤醒
    usleep(3 * 200 * 1000);
    check(c->resumes == 1, "fd超时: 无二次唤醒(监听已取消), resumes=" + std::to_string(c->resumes.load()));
    closeFds(fds);
}

// 用例3: 数据在超时前到达, 正常读取
static void testReadBeforeTimeout(IOManager* iom)
{
    int fds[2] = {-1, -1};
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
    {
        check(false, "就绪读取: socketpair 创建失败");
        return;
    }
    auto c = std::make_shared<ReadCase>();
    c->fd = fds[0];
    c->peer_fd = fds[1];
    c->fd_timo_us = 2000 * 1000;        // 2s, 远大于数据到达时间
    c->peer_write_delay_ms = 100;
    c->peer_write_len = 5;
    iom->schedule(TaskCoro20::create_coro([c]() { return co_readCase(c); }));

    bool ok = waitValue(c->done, 0, 3000);
    check(ok, "就绪读取: 协程被唤醒");
    if(ok)
    {
        check(c->rt == 5, "就绪读取: 读到5字节, 实际=" + std::to_string(c->rt.load()));
        check(inRange(c->cost_ms, 80, 1000),
              "就绪读取: 耗时<超时时间, 实际=" + std::to_string(c->cost_ms.load()) + "ms");
    }
    closeFds(fds);
}

// 用例4: co_connect 就绪路径不被误判为超时
static void testConnectReady(IOManager* iom, int listen_fd, uint16_t port)
{
    auto c = std::make_shared<ConnectCase>();
    c->port = port;
    iom->schedule(TaskCoro20::create_coro([c]() { return co_connectCase(c); }));

    bool ok = waitValue(c->done, 0, 6000);
    check(ok, "connect就绪: 协程被唤醒");
    if(ok)
    {
        check(c->rt == 0, "connect就绪: 返回0(未被误判为超时), 实际=" + std::to_string(c->rt.load())
                          + " errno=" + std::to_string(c->errno_val.load()));
    }
    (void)listen_fd;
}

// 用例5: co_connect 失败路径
static void testConnectRefused(IOManager* iom)
{
    uint16_t port = getFreePort();
    auto c = std::make_shared<ConnectCase>();
    c->port = port;
    iom->schedule(TaskCoro20::create_coro([c]() { return co_connectCase(c); }));

    bool ok = waitValue(c->done, 0, 6000);
    check(ok, "connect拒绝: 协程被唤醒");
    if(ok)
    {
        check(c->rt == -1, "connect拒绝: 返回-1, 实际=" + std::to_string(c->rt.load())
                           + " errno=" + std::to_string(c->errno_val.load()));
    }
}

// 用例6: co_connect 到不可达地址, 不允许返回伪成功
static void testConnectUnreachable(IOManager* iom)
{
    auto c = std::make_shared<ConnectCase>();
    c->ip = "10.255.255.1";
    c->port = 80;
    iom->schedule(TaskCoro20::create_coro([c]() { return co_connectCase(c); }));

    bool ok = waitValue(c->done, 0, 8000);
    check(ok, "connect不可达: 协程被唤醒");
    if(ok)
    {
        // 可能因无路由立即失败, 也可能等待connect超时, 但都不允许返回0
        check(c->rt != 0, "connect不可达: 未返回伪成功, rt=" + std::to_string(c->rt.load())
                          + " errno=" + std::to_string(c->errno_val.load()));
    }
}

int main(void)
{
    std::cout << "HOOK_IOAWAIT_TIMEOUT = " << HOOK_IOAWAIT_TIMEOUT << "ms" << std::endl;
    m_sylar::IOManager iom("hook_timeout_test", 2);

    testDefaultTimeout(&iom);
    testFdTimeout(&iom);
    testReadBeforeTimeout(&iom);

    uint16_t port = 0;
    int listen_fd = makeListenSocket(&port);
    if(listen_fd < 0)
    {
        check(false, "connect就绪: 创建监听socket失败");
    }
    else
    {
        testConnectReady(&iom, listen_fd, port);
    }
    testConnectRefused(&iom);
    testConnectUnreachable(&iom);

    closeFd(listen_fd);

    iom.stop();
    std::cout << (g_failed ? "===== 存在问题, 失败用例数: " : "===== 全部用例通过, 失败用例数: ")
              << g_failed << std::endl;
    return g_failed ? 1 : 0;
}
