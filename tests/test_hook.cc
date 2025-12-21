#include "../sylar/hook.h"
#include "../sylar/ioManager.h"
#include <cstring>
#include <ctime>
#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>

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

void test2(void)
{

    M_SYLAR_LOG_DEBUG(g_logger) << "begin";
    m_sylar::IOManager::ptr iom (new m_sylar::IOManager("test_hook", 1));

    iom->schedule([](){
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        inet_pton(AF_INET, "111.63.65.103", &addr.sin_addr.s_addr);

        int rt = connect(sockfd, (sockaddr*)&addr, sizeof(addr));

        M_SYLAR_LOG_INFO(g_logger) << "connect rt = {" << rt << "}, errno = {" << errno << ", " << strerror(errno) << "}";
    });


    original_sleep(1);
    iom->stop();
}

int main(void)
{
    test2();
    return 0;
}