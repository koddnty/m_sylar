#include "coroutine/coro20/task.hpp"
#include<iostream>
#include <ostream>

using namespace m_sylar;


Task<int> simple_task2() {
  std::cout << "task 2 start ..." << std::endl;
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1s);
  std::cout << "task 2 returns after 1s." << std::endl;
  co_return 2;
}

Task<int> simple_task3() {
  std::cout << "in task 3 start ..."  << std::endl;
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);
  std::cout << "task 3 returns after 2s." << std::endl;
  co_return 3;
}

Task<int> simple_task() {
  std::cout << "task start ..."  << std::endl;
  auto result2 = co_await simple_task2();
  std::cout << "returns from task2: " << result2  << std::endl;
  auto result3 = co_await simple_task3();
  std::cout << "returns from task3: " << result3  << std::endl;
  co_return 1 + result2 + result3;
}

int main(void)
{
    Task<int> t = simple_task();
    std::cout << t.getResult() << std::endl;

    return 0;
}
