#include "basic/allHeader.h"
#include "coroutine/corobase.h"
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <netinet/in.h>
#include <ostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


int count = 0;
void test1_fiber1()
{
    count++;
}

void test1()
{
    m_sylar::IOManager iom ("test_ioManager", 1);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "create socket failed"; 
        return;
    }
    
    M_SYLAR_LOG_DEBUG(g_logger) << "socketfd = " << sockfd;
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(80);
    inet_pton(AF_INET, "111.63.65.247", &server_addr.sin_addr);



    M_SYLAR_LOG_INFO(g_logger) << " 添加events";
    iom.addEvent(sockfd, m_sylar::IOManager::Event::READ, [sockfd](){
        M_SYLAR_LOG_INFO(g_logger) << "检测到一个读事件， fd:" << sockfd;
    });

    // sleep(5);

    iom.addEvent(sockfd, m_sylar::IOManager::Event::WRITE, [sockfd](){
        M_SYLAR_LOG_INFO(g_logger) << "检测到一个写事件， fd:" << sockfd;
    });


    // M_SYLAR_LOG_INFO(g_logger) << " events添加完成";

    M_SYLAR_LOG_INFO(g_logger) << " 连接开始";
    int ct_rt = connect(sockfd, (sockaddr*) &server_addr, sizeof(server_addr));
    M_SYLAR_LOG_INFO(g_logger) << " 连接完毕";


    std::cout << "睡了" << std::endl;
    sleep(2);
    iom.stop();
    return;
}   

void test2()
{
    m_sylar::IOManager iom ("test_ioManager", 2);
    sleep(1);
    iom.schedule(test1_fiber1);
    sleep(2);
    iom.autoStop();
}

void test3()
{
    m_sylar::IOManager iom ("test_ioManager", 10);
    for(int i = 0; i < 1000; i++)
    {
        iom.schedule(test1_fiber1);
    }
    sleep(2);
    iom.stop();
    std::cout << count << std::endl;

}

int main(void) 
{
    test1();
    return  0;
}
