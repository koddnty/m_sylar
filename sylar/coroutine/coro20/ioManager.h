#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/scheduler.h"
#include "basic/fdManager.h"
#include "basic/log.h"
#include <cstdint>
#include <memory>
#include <sys/epoll.h>
#include <vector>

namespace m_sylar
{


class IOManager : public Scheduler
{
public:
    using ptr = std::shared_ptr<IOManager>;

    enum Event
    {   
        NONE = 0x0,
        READ = EPOLLIN,
        WRITE = EPOLLOUT
    };

    // IOManager(Scheduler* scheduler = Scheduler::GetThis());
    IOManager(const std::string& name, int thread_num = 1);
    ~IOManager();

    int addEvent(int fd, Event event, TaskCoro20 task);
    int addEvent(int fd, Event event, std::function<void()> cb_func = nullptr);     // 添加事件

    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);
    bool cancelAll(int fd);

    static IOManager* getInstance();

  
private:
    void idle() override;
    void tickle() override;
    

private:
    class FdContext
    {
    public: 
        using ptr = std::shared_ptr<FdContext>;
        FdContext(int fd);
        ~FdContext();
        
        // void reset();
        void trigger(Event event);

    public:
        int m_fd;
        Event m_event;
        TaskCoro20 m_cb_read;
        TaskCoro20 m_cb_write;
    };

    int contextSet(int fd, FdContext::ptr fd_ctx);

private:
    int m_eventFd;
    int m_epollFd;
    // Scheduler* m_scheduler;
    std::vector<FdContext::ptr> m_fd_contexts;
};

}