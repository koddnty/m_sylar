// #include "basic/hook.h"
#include "coroutine/corobase.h"
// #include "basic/ioManager.h"
#include <cstring>
#include <ctime>
#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

void test1()
{
    M_SYLAR_LOG_DEBUG(g_logger) << "begin";
    m_sylar::IOManager::ptr iom (new m_sylar::IOManager("test_hook", 1));
    iom->schedule([](){
        sleep(2);
        M_SYLAR_LOG_DEBUG(g_logger) << "sleep 2 sec";
    });

    iom->schedule([](){
        usleep(3000000);
        M_SYLAR_LOG_DEBUG(g_logger) << "sleep 3 sec";
    });

    iom->schedule([](){
        timespec timer;
        timer.tv_nsec = 10000000000;
        timer.tv_sec = 5;
        nanosleep(&timer, NULL);
        M_SYLAR_LOG_DEBUG(g_logger) << "sleep 5s-0n";
    });

    original_sleep(20);
    // sleep(2000);
    M_SYLAR_LOG_DEBUG(g_logger) << "end";
}

void test2_withSchedule(void)
{

    M_SYLAR_LOG_DEBUG(g_logger) << "begin";
    m_sylar::IOManager::ptr iom (new m_sylar::IOManager("test_hook", 12));

    iom->schedule([](){
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        inet_pton(AF_INET, "111.63.65.247", &addr.sin_addr.s_addr);

        int rt = connect(sockfd, (sockaddr*)&addr, sizeof(addr));

        M_SYLAR_LOG_INFO(g_logger) << "connect rt = {" << rt << "}, errno = {" << errno << ", " << strerror(errno) << "}";

        const char send_data[] = "GET / HTTP/1.0\r\n\r\n";
        rt = send(sockfd, send_data, sizeof(send_data), 0);
        M_SYLAR_LOG_INFO(g_logger) << "send rt = {" << rt << "}, errno = {" << errno << ", " << strerror(errno) << "}";

        
        std::string recv_buffer;
        recv_buffer.resize(4096);
        rt = recv(sockfd, &recv_buffer[0], recv_buffer.size(), 0);
        M_SYLAR_LOG_INFO(g_logger) << "recv rt = {" << rt << "}, errno = {" << errno << ", " << strerror(errno) << "}";

        if(rt <= 0)
        {
            return;
        }
        recv_buffer.resize(rt);
        M_SYLAR_LOG_DEBUG(g_logger) << recv_buffer;

    });

    original_sleep(100);
    iom->stop();
}

void test2()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "111.63.65.247", &addr.sin_addr.s_addr);

    int rt = connect(sockfd, (sockaddr*)&addr, sizeof(addr));

    M_SYLAR_LOG_INFO(g_logger) << "connect rt = {" << rt << "}, errno = {" << errno << ", " << strerror(errno) << "}";

    const char send_data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sockfd, send_data, sizeof(send_data), 0);
    M_SYLAR_LOG_INFO(g_logger) << "send rt = {" << rt << "}, errno = {" << errno << ", " << strerror(errno) << "}";


    std::string recv_buffer;
    recv_buffer.resize(4096);
    rt = recv(sockfd, &recv_buffer[0], recv_buffer.size(), 0);
    M_SYLAR_LOG_INFO(g_logger) << "recv rt = {" << rt << "}, errno = {" << errno << ", " << strerror(errno) << "}";

    if(rt <= 0)
    {
    return;
    }
    recv_buffer.resize(rt);
    M_SYLAR_LOG_DEBUG(g_logger) << recv_buffer;
}

int main(void)
{
    test2_withSchedule();
    // test2();
    return 0;
}