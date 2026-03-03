#include <iostream>
#include <iterator>
#include <memory>
#include <sys/socket.h>
#include "basic/socket.h"
#include "basic/address.h"
// #include "basic/ioManager.h"
#include "coroutine/corobase.h"

int test_socket()
{
    m_sylar::IOManager::ptr iom (new m_sylar::IOManager("test_hook", 12));

    iom->schedule([](){
        m_sylar::Address::ptr baidu_address = m_sylar::Address::LookupAnyIPAddress("www.baidu.com", AF_INET, 0, 0);

        if(!std::dynamic_pointer_cast<m_sylar::IPAddress>(baidu_address))
        {
            std::cout << "dynamic cast failed\n";
        }

        std::dynamic_pointer_cast<m_sylar::IPv4Address>(baidu_address)->setPort(80);

        std::cout << baidu_address->toString() << std::endl;

        m_sylar::Socket::ptr baidu_sock = m_sylar::Socket::CreateTCP(baidu_address);

        std::cout << "connecting\n";
        baidu_sock->connect(baidu_address);

        int buffer_size = 512;
        char buffer[512] = "GET / HTTP/1.0\r\n\r\n";
        
        baidu_sock->send(buffer, sizeof(buffer));
        std::cout << "after send\n";

        std::cout << "buffer : \n";
        while(baidu_sock->recv(buffer, buffer_size) > 0)
        {
            std::cout  << buffer << std::endl;
        }
        
        std::cout << "after recv\n";
    });

    sleep(10);
    iom->stop();
    sleep(10);
    return 0;
}


int main(void)
{
    test_socket();
    return 0;
}