#include "coroutine/coro20/task.hpp"
#include "basic/log.h"
#include "coroutine/coro20/fiber.h"
// #include "coroutine/coro20/ioManager.h"
#include <iostream>
#include <ostream>

#include "basic/self_timer.h"

using namespace m_sylar;
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


class SleepAwaiter : public Awaiter<int>
{
public:
  SleepAwaiter(uint64_t time)
  {
    m_time = time;
  }

protected:
  void on_suspend() override
  {
    m_sylar::TimeManager* tim = m_sylar::TimeManager::getInstance();
    int fd = tim->addTimer(m_time * 1000000, false, [this](){
      std::cout << "SleepAwatier call back begin " << this << std::endl;
      resume(m_time);
      std::cout << "SleepAwatier call back end" << std::endl;
    });
    std::cout << "on_suspend, time fd = " << fd << std::endl;
  }

  void before_resume() override
  {
    std::cout << "before_resume" << std::endl;
  }

private:
  uint64_t m_time;
};

Task<void> msleep(uint64_t time)
{
  std::cout << "[msleep sleep 10s begin]" << std::endl;
  co_await SleepAwaiter(time);
  std::cout << "[msleep sleep 10s end]" << std::endl;
  co_return ;
} 

Task<int> nothing()
{
  std::cout << "[msleep sleep 0s begin]" << std::endl;
  std::cout << "[msleep sleep 0s end]" << std::endl;
  co_return 1;
} 

Task<void, TaskBeginExecuter> coro()
{
  std::cout << "coroutine task running(with a async sleep task)..." << std::endl;
  co_await msleep(3);
  // co_await nothing();
  // // co_await SleepAwaiter(3);
  std::cout << "coroutine task end, after 3s" << std::endl;
  co_return;
}


void func()
{
  std::cout << "function task running..." << std::endl;
  std::cout << "function task running end" << std::endl;
  return;
}


void test_C()
{
  // TaskCoro20 t(static_cast<std::function<Task<void, TaskBeginExecuter>()>>(coro));
  TaskCoro20 t(TaskCoro20::create_coro(coro));
  std::cout << "coroutine task start" << std::endl;
  t.start();
  std::cout << "coroutine task started" << std::endl;
  sleep(10);
  return;
}

void test_F()
{
  TaskCoro20 t(static_cast<std::function<void()>>(func));
  std::cout << "function task started" << std::endl;
  t.start();
  std::cout << "function task started" << std::endl;
  // sleep(5);
  return;
}

void test_task()
{
  std::cout << "------test function task" << std::endl;
  test_F();
  std::cout << "------test coroutine task" << std::endl;
  test_C();
  // sleep(10);
  std::cout << "------test end------" << std::endl;
  return;
}

void test_twice_coro()
{
  test_C();
  std::cout << "test_C returned" << std::endl;
}

int main(void)
{
  m_sylar::IOManager::ptr iom (new m_sylar::IOManager("timer_test", 1));
  TimeManager tim (iom);
  // test_void();
  // test_resume();
  // test_awaiter();
  // test_F();
  test_twice_coro();
  std::cout << "test_task returned, but task still running..." << std::endl;
  sleep(15);
  iom->autoStop();
  std::cout << "autoStoped" << std::endl;



  // if(t.getHandle().done())
  // {
  //   std::cout << "运行完毕， result:";
  //   std::cout << t.getResult() << std::endl;
  // }
  // else
  // {
  //   std::cout << "协程挂起， result:";
  //   t.getHandle().resume();
  //   std::cout << t.getResult() << std::endl;
  // }
  return 0;
}
