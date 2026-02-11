#include "coroutine/coro20/task.hpp"
#include "coroutine/coro20/ioManager.h"
#include <iostream>
#include <ostream>

using namespace m_sylar;


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

// 测试awaiter
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
    sleep(m_time);
    resume(10);
  }

  void before_resume() override
  {

  }
  
private: 
  uint64_t m_time;
};

Task<int> test()
{
  std::cout << "begin" << std::endl;
  int value = co_await SleepAwaiter(10);
  std::cout << "ok : " << value << std::endl;
}

void test_awaiter()
{
  Task<int> t = test();
  return;
}


int main(void)
{
  // test_void();
  // test_resume();
  test_awaiter();

  
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
