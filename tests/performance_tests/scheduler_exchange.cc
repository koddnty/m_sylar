// #include "basic/ioManager.h"
#include "coroutine/corobase.h"
#include <iostream>

int count = 0;

void func()
{
    count++;
    // std::cout << 1;
}

int main(void)
{
    m_sylar::IOManager iom("test_scheduler", 12);
    int total = 10000;
    // int total = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < total; i++)
    {
        iom.schedule(func);
    }

    auto end = std::chrono::high_resolution_clock::now(); 
    std::cout << "task count" << iom.getTaskCount() << std::endl;
    std::cout << "duration time : " << (end - start).count();
    // iom.schedule(func);
    int countbegin = count;
    std::cout << "begin : ----\n";
    int sec = 15;
    // sleep(sec);
    // sleep(1);
    // std::cout << "task count" << iom.getTaskCount() << std::endl;
    // sleep(2);
    // std::cout << "task count" << iom.getTaskCount() << std::endl;
    // sleep(4);
    // std::cout << "task count" << iom.getTaskCount() << std::endl;

    sleep(sec);
    std::cout << "end : ----" << countbegin << "  " << count << "\n";
    std::cout << "count " << count - countbegin << " in " << sec << "secs";  

    iom.autoStop();
    
    return 0;
}