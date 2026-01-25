#pragma once
#include <cstdint>
#include <memory.h>
#include <memory>
#include <pthread.h>
#include <sys/socket.h>
#include <vector>
#include "basic/ioManager.h"    
#include "basic/socket.h"
#include "basic/noncopyable.h"
#include "basic/log.h"
#include "basic/socket.h"
#include "basic/address.h"



namespace m_sylar
{

class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable
{
public:
    using ptr = std::shared_ptr<TcpServer>;

    TcpServer(IOManager* io_manager = IOManager::getInstance());
    virtual ~TcpServer();

    virtual bool bind(Address::ptr addr);
    virtual bool bind(std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& failed);
    virtual bool start();
    virtual bool stop();

    uint64_t getRTimeout() const { return m_readtimeout;}
    std::string getName() const { return m_name;}
    void setRtimeout(uint64_t);
    void setName(std::string name);

    bool isStop() const { return m_stop;}

protected:
    virtual void handleClient(Socket::ptr client);
    virtual void startAccept(Socket::ptr sock);

private:
    std::vector<Socket::ptr> m_sockets;
    IOManager* m_iomanager;
    uint64_t m_readtimeout;
    std::string m_name;
    bool m_stop;
};

}