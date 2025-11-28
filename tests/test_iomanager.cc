#include "../sylar/allHeader.h"
#include "../sylar/ioManager.h"
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


void test1_fiber1()
{
    M_SYLAR_LOG_INFO(g_logger) << "test1_fiber1";
    std::cout << "test1_fiber1";
}

void test1()
{
    m_sylar::IOManager iom ("test_ioManager");
    iom.schedule(test1_fiber1);
    // sleep(3);
    // M_SYLAR_LOG_INFO(g_logger) << "开始批量添加事件";
    // for(int i = 0; i < 1; i++){
    //     iom.schedule(test1_fiber1);
    // }
    // sleep(20);
    // iom.stop();

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        M_SYLAR_LOG_ERROR(g_logger) << "create socket failed"; 
        return;
    }
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(80);
    inet_pton(AF_INET, "111.63.65.247", &server_addr.sin_addr);




    M_SYLAR_LOG_INFO(g_logger) << " 添加events";
    iom.addEvent(sockfd, m_sylar::IOManager::Event::READ, [sockfd](){
        M_SYLAR_LOG_INFO(g_logger) << "检测到一个读事件， fd:" << sockfd;
    });

    iom.addEvent(sockfd, m_sylar::IOManager::Event::WRITE, [sockfd](){
        M_SYLAR_LOG_INFO(g_logger) << "检测到一个写事件， fd:" << sockfd;
        close(sockfd);
    });

    M_SYLAR_LOG_INFO(g_logger) << " events添加完成";

    int ct_rt = connect(sockfd, (sockaddr*) &server_addr, sizeof(server_addr));
    M_SYLAR_LOG_INFO(g_logger) << " 连接完毕";

    const std::string message = "helloWorld\n";
    // ssize_t message_len = send(sockfd, message.c_str(), message.length(), 0);
    // M_SYLAR_LOG_INFO(g_logger) << " 信息发送完毕";

    std::cout << "睡了" << std::endl;
    sleep(3);
    iom.stop();
    return;
}   

void client()
{


}

int main(void) 
{
    
    test1();
    return  0;
}
