#include "basic/ioManager.h"
#include "basic/scheduler.h"
#include "lab/fiber.h"
#include "schedule.h"
#include "basic/fdManager.h"
#include "basic/log.h"
#include <cstdint>
#include <memory>
#include <sys/epoll.h>
#include <vector>

namespace m_sylar
{


class IOManagerCoro20 : public SchedulerCoro20
{
public:
    using ptr = std::shared_ptr<IOManagerCoro20>;

    enum Event
    {   
        NONE = 0,
        READ = EPOLLIN,
        WRITE = EPOLLOUT
    };

    // IOManagerCoro20(SchedulerCoro20* scheduler = SchedulerCoro20::GetThis());
    IOManagerCoro20(const std::string& name, int thread_num = 1);
    ~IOManagerCoro20();

    void addEvent(int fd, Event event, TaskCoro20 task);
    void delEvent(int fd, Event event);
    void cancelEvent(int fd, Event event);
    void cancelAll(int fd);

  
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
    // SchedulerCoro20* m_scheduler;
    std::vector<FdContext::ptr> m_fd_contexts;
};

}