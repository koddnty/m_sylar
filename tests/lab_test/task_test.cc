#include "coroutine/coro20/task.hpp"
#include "basic/log.h"
// #include "coroutine/coro20/ioManager.h"
#include <iostream>
#include <ostream>

#include "basic/self_timer.h"

using namespace m_sylar;
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


// Task<int> simple_task2() {
//   std::cout << "task 2 start ..." << std::endl;
//   using namespace std::chrono_literals;
//   std::this_thread::sleep_for(1s);
//   std::cout << "task 2 returns after 1s." << std::endl;
//   co_return 2;
// }

// Task<int> simple_task3() {
//   std::cout << "in task 3 start ..."  << std::endl;
//   using namespace std::chrono_literals;
//   std::this_thread::sleep_for(2s);
//   std::cout << "task 3 returns after 2s." << std::endl;
//   co_return 3;
// }




// Task<int> simple_task() {
//   std::cout << "task start ..."  << std::endl;
//   auto result2 = co_await simple_task2();
//   std::cout << "returns from task2: " << result2  << std::endl;
//   auto result3 = co_await simple_task3();
//   std::cout << "returns from task3: " << result3  << std::endl;
//   co_return 1 + result2 + result3;
// }

// template<typename _ResultType = int>

// class SleepExecuter : public AbstractExecutor
// {
// public:
//     virtual void execute(std::function<void()>&& func) override
//     { // 处理协程恢复逻辑
//       // 注册事件到iomanager
//       getMainFiber().resume();    // 恢复到主协程

//         func();
//     }

// private:
//     void callback()
//     {
//       func();
//     }
// };

// template<>
// class m_sylar::TaskAwaiter<int, BaseExecutor>
// {
// public:
//     TaskAwaiter(Task<int, BaseExecutor>&& task, BaseExecutor* executer)
//         : m_task(std::move(task)), m_executer(executer) {}

// public:
//     virtual bool await_ready()
//     {
//         return false;
//     }

//     virtual void await_suspend(std::coroutine_handle<TaskPromise<int, BaseExecutor>> handle)
//     {
//       std::cout << "uo" << std::endl;
//         // 实现协程恢逻辑
//         // m_task.finally([handle](Result<_ResultType> result){
//         //     handle.resume();
//         // });

//         // m_executer->execute([handle](){
//         //     handle.resume();
//         // });
//     }

//     int await_resume()
//     {
//         return m_task.getResult();
//     }

// private:
//     Task<int, BaseExecutor> m_task;       // 子协程任务
//     BaseExecutor* m_executer;           // 调度器
// };


// Task<int, SleepExecuter> sleepTask(uint64_t time)
// {
//   m_sylar::IOManager::ptr iom (new m_sylar::IOManager("timer_test", 12));
//   m_sylar::TimeManager::ptr tim (new m_sylar::TimeManager(iom));
//   tim->addTimer(time, false, std::function<void ()> main_cb);    // 注册恢复函数任务

// }

// 测试void篇特化是否正常
// Task<void, BaseExecutor> void2()
// {
//   co_return;
// }

// Task<void, BaseExecutor> void1()
// {
//   co_await void2();
//   co_return;
// }

// void test_void()
// {
//   Task<void, BaseExecutor> t = void1();
//   t.getResult();
//   std::cout << "endl : " << std::endl;
// }



// 测试协程控制流
// static std::coroutine_handle<> main_coro;

// class SleepExecuter : public AbstractExecuter
// {
// public:
//   virtual void execute(std::function<void()> &&func) override
//   {
//     std::cout << "SleepExecuter " << std::endl;
//     sleep(10);
//     func();
//   }
// };

// class NoneExecuter : public AbstractExecuter
// {
// public:
//   virtual void execute(std::function<void()> &&func) override
//   {
//     std::cout << "NoneExecuter" << std::endl;
//   }
// };




// Task<int, SleepExecuter> C(int time = 10)
// {
//   // 先出一个任务，注册恢复自己的任务，挂起自己
//   std::cout << "C begin 1" << std::endl;
//   co_return time;
// }

// Task<int, BaseExecutor> B()
// {
//   std::cout << "B begin 1" << std::endl;
//   // int value = co_await C();   // 用Main调度器，直接回到M，B丢失
//   // auto t = C();
//   int value = co_await C(1);
//   // int value = t.getResult();
//   std::cout << "B 2" << std::endl;
//   co_return value;
// }

// Task<void, NoneExecuter> stop()
// {
//   std::cout << "stop 1" << std::endl;
//   co_return;
// }

// Task<void, BaseExecutor> M()
// {
//   std::cout << "mainFIber 1" << std::endl;
//   co_await stop();
//   std::cout << "co_await B" << std::endl;
//   int value = co_await B();
//   std::cout << "M: resumed" << std::endl;
// }

// void test_resume()
// {
//   Task<void, BaseExecutor> t = M();
//   std::cout << "test_resume 1" << std::endl;
//   main_coro = t.getHandle();
//   std::cout << "test_resume 2" << std::endl;
//   t.getHandle().resume();
//   std::cout << "test_resume 3" << std::endl;
//   std::cout << "end of test_resume" << std::endl;
// }

// // ---------------------
// 测试awaiter
// class TaskBeginExecuter : public AbstractExecuter
// { // 此调度器在任务创建时挂起任务， 执行期间不挂起任务
// public:
//     virtual void execute(std::function<void()> &&func)
//     {
//         func();
//     }

//     virtual void initialExecute(std::function<void()> &&func)
//     {
//     }
// };

// class SleepAwaiter : public Awaiter<int>
// {
// public:
//   SleepAwaiter(uint64_t time)
//   {
//     m_time = time;
//   }

// protected:
//   void on_suspend() override
//   {
//     m_sylar::TimeManager* tim = m_sylar::TimeManager::getInstance();
//     tim->addTimer(m_time * 1000000, false, [this](){
//       std::cout << "SleepAwatier call back begin " << this << std::endl;
//       resume(m_time);
//       std::cout << "SleepAwatier call back end" << std::endl;
//     });
//     std::cout << "on_suspend" << std::endl;
//   }

//   void before_resume() override
//   {
//     std::cout << "before_resume" << std::endl;
//   }

// private:
//   uint64_t m_time;
// };

// Task<int> Sleep(uint64_t time)
// {
//   std::cout << "begin" << std::endl;
//   int value = co_await SleepAwaiter(time);
//   std::cout << "ok : " << value << std::endl;
// }

// Task<void, TaskBeginExecuter> m_sleep(uint64_t time)
// {
//   std::cout << "sleep 10s begin" << std::endl;
//   co_await Sleep(time);
//   std::cout << "sleep 10s end" << std::endl;
//   co_return;
// }

// void test_awaiter()
// {
//   // std::cout << "sleep 10s begin" << std::endl;
//   auto t = m_sleep(2);
//   std::cout << "resume t task" << std::endl;
//   t.getHandle().resume();
//   std::cout << "m_sleep back" << std::endl;

//   sleep(10);
//   return;
// }


// -----------
// 测试任务封装


// class SleepAwaiter : public Awaiter<int>
// {
// public:
//   SleepAwaiter(uint64_t time)
//   {
//     m_time = time;
//   }

// protected:
//   void on_suspend() override
//   {
//     m_sylar::TimeManager* tim = m_sylar::TimeManager::getInstance();
//     tim->addTimer(m_time * 1000000, false, [this](){
//       std::cout << "SleepAwatier call back begin " << this << std::endl;
//       resume(m_time);
//       std::cout << "SleepAwatier call back end" << std::endl;
//     });
//     std::cout << "on_suspend" << std::endl;
//   }

//   void before_resume() override
//   {
//     std::cout << "before_resume" << std::endl;
//   }

// private:
//   uint64_t m_time;
// };

// Task<void> msleep(uint64_t time)
// {
//   std::cout << "sleep 10s begin" << std::endl;
//   co_await SleepAwaiter(time);
//   std::cout << "sleep 10s end" << std::endl;
//   co_return;
// }


// class TaskBeginExecuter : public AbstractExecuter
// { // 此调度器在任务创建时挂起任务， 执行期间不挂起任务
// public:
//     virtual void execute(std::function<void()> &&func)
//     {
//         func();
//     }

//     virtual void initialExecute(std::function<void()> &&func)
//     {
//     }
// };

// class F
// {
// public:
//   F(std::function<Task<void, TaskBeginExecuter>()> task)
//     : m_task(task()){}

//   F(std::function<void()> task)
//     :  m_func_task(task), m_task(std::bind(&F::runner, this)()){ }

//   void start()
//   {
//     if(m_task.getHandle().done())
//     {
//       M_SYLAR_LOG_ERROR(g_logger) << "[coroutine]resume a finished handle";
//     }
//     m_task.getHandle().resume();
//   }

// private:
//   Task<void, TaskBeginExecuter> runner()
//   {
//     m_func_task();
//     co_return;
//   };

// private:
//   std::function<void()> m_func_task = nullptr;
//   Task<void, TaskBeginExecuter> m_task;
// };

// Task<void, TaskBeginExecuter> coro()
// {
//   std::cout << "sleep 3s" << std::endl;
//   co_await msleep(3);
//   std::cout << "after sleep 3s, func finished" << std::endl;
//   co_return;
// }


// void func()
// {
//   std::cout << "func runing" << std::endl;
// }


// void test_C()
// {
//   F t(static_cast<std::function<Task<void, TaskBeginExecuter>()>>(coro));
//   std::cout << "task start" << std::endl;
//   t.start();
//   std::cout << "task started" << std::endl;

//   sleep(10);
// }

// void test_F()
// {
//   F t(static_cast<std::function<void()>>(coro));
//   t.start();
//   std::cout << "task started" << std::endl;
// }


// -------------------
class TaskBeginExecuter : public AbstractExecuter
{ // 此调度器在任务创建时挂起任务， 执行期间不挂起任务
public:
    virtual void execute(std::function<void()> &&func)
    {
        func();
    }

    virtual void initialExecute(std::function<void()> &&func)
    {
    }
};

class SuspendExecuter : public AbstractExecuter
{ // 此调度器在任务创建时挂起任务， 执行期间不挂起任务
public:
    virtual void execute(std::function<void()> &&func)
    {
        // func();
        if(is_resumed)
        {
          std::cout << "one executer can not resume a handle twice" << this << std::endl;
          return;
        }
        std::cout << "SuspendExecuter: " << this << std::endl;
        func();
        is_resumed = true;
    }

    virtual void initialExecute(std::function<void()> &&func)
    {
      std::cout << "SuspendintitalExecuter : " << this << std::endl;
      func();
    }

private:
    bool is_resumed = false;
};

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
    tim->addTimer(m_time * 1000000, false, [this](){
      std::cout << "SleepAwatier call back begin " << this << std::endl;
      resume(5);
      std::cout << "SleepAwatier call back end" << std::endl;
    });

    // std::cout << "SleepAwatier call back begin " << this << std::endl;
    // resume(1);
    // std::cout << "SleepAwatier call back end" << std::endl;

    std::cout << "on_suspend" << std::endl;
  }

  void before_resume() override
  {
    std::cout << "before_resume" << std::endl;
  }

private:
  uint64_t m_time;
};

Task<void, SuspendExecuter> misleep(uint64_t time)
{
  std::cout << "misleep sleep " << time << "s begin" << std::endl;
  co_await SleepAwaiter(time);    // 刮起后
  std::cout << "---- misleep sleep " << time << "s misleep end" << std::endl;
  co_return ;
}


// Task<void> Sleep(uint64_t time)
// {
//   std::cout << "begin" << std::endl;
//   int value = co_await SleepAwaiter(time);
//   std::cout << "ok : " << value << std::endl;
//   co_return ;
// }

// Task<void> misleep(uint64_t time)
// {
//   std::cout << "sleep 10s begin" << std::endl;
//   co_await Sleep(time);
//   std::cout << "sleep 10s end" << std::endl;
//   co_return ;
// }


class F
{
public:
  F(std::function<Task<void, TaskBeginExecuter>()> task)
    : m_task(task())
    {
      std::cout << "协程任务构造函数" << std::endl;
    }

  F(std::function<void()> task)
    :  m_func_task(task), m_task(std::bind(&F::runner, this)()){ }

  void start()
  {
    if(m_task.getHandle().done())
    {
      M_SYLAR_LOG_ERROR(g_logger) << "[coroutine]resume a finished handle";
    }
    m_task.getHandle().resume();
  }

private:
  Task<void, TaskBeginExecuter> runner()
  {
    m_func_task();
    co_return;
  };

private:
  std::function<void()> m_func_task = nullptr;
  Task<void, TaskBeginExecuter> m_task;
};


Task<int, TaskBeginExecuter> func()
{ 
  std::cout << "sleep 3s" << std::endl;
  co_await misleep(5);      // 刮起后，给misleep的finally加入resume自己的任务
  std::cout << "------ after sleep 3s, func return" << std::endl;
  co_return 1;
}


Task<void, TaskBeginExecuter> test_live()
{
  std::cout << "sleep 3s" << std::endl;
  co_await misleep(5);
  std::cout << "------ after sleep 3s, test_live return" << std::endl;
  co_return ;
} 


void test_F()
{
  // F t(static_cast<std::function<Task<void, TaskBeginExecuter>()>>(func));
  // std::cout << "test_F start" << std::endl;
  // t.start();
  // std::cout << "test_F started" << std::endl;


  // auto t = func();
  // std::cout << "resume t task" << std::endl;
  // t.getHandle().resume();
  // std::cout << "resumed" << std::endl;

  auto t = test_live();

  t.getHandle().resume();

  std::cout << "tetst_F sleep 4s" << std::endl;
  sleep(10);
  std::cout << "after tetst_F sleep 4s" << std::endl;
  std::cout << "get task result" << std::endl;
  // t.getResult(); 

  sleep(1200);
  std::cout << "test_F end" << std::endl;
  return;
}



int main(void)
{
  m_sylar::IOManager::ptr iom (new m_sylar::IOManager("timer_test", 1));
  TimeManager tim (iom);
  // test_void();
  // test_resume();
  // test_awaiter();
  // test_F();
  test_F();




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
