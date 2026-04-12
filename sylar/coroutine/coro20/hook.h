#pragma once
#include <dlfcn.h>
#include <sys/socket.h>
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/ioManager.h"
#include "basic/self_timer.h"

namespace m_sylar
{
#define HOOK_IOAWAIT_TIMEOUT 5000000            // ioawait超时时间

    bool is_hook_enable();
    void set_hook_state(bool flags);

m_sylar::Task<unsigned int> co_sleep(unsigned int seconds);

m_sylar::Task<int> co_usleep(useconds_t usec);

m_sylar::Task<int> co_nanosleep(const struct timespec *duration,
                struct timespec* rem);

int co_socket(int domain, int type, int protocol);

m_sylar::Task<ssize_t> co_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

m_sylar::Task<int> co_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

m_sylar::Task<ssize_t> co_read(int fd, void* buf, size_t count);

m_sylar::Task<ssize_t > co_readv(int fd, const struct iovec *iov, int iovcnt);

m_sylar::Task<ssize_t > co_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);

m_sylar::Task<ssize_t> co_preadv2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);

m_sylar::Task<ssize_t> co_recv(int sockfd, void* buf, size_t len, int flags);

m_sylar::Task<ssize_t> co_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr *  src_addr, socklen_t* addrlen);

m_sylar::Task<ssize_t> co_recvmsg(int sockfd, struct msghdr *msg, int flags);

m_sylar::Task<ssize_t> co_write(int fd, const void* buf, size_t count);

m_sylar::Task<ssize_t> co_writev(int fd, const struct iovec *iov, int iovcnt);

m_sylar::Task<ssize_t> co_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);

m_sylar::Task<ssize_t> co_pwritev2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);

m_sylar::Task<ssize_t> co_send(int sockfd, const void* buf, size_t len, int flags);

m_sylar::Task<ssize_t> co_sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);

m_sylar::Task<ssize_t> co_sendmsg(int sockfd, const struct msghdr *msg, int flags);

int co_close(int fd, int mod = 0);

int co_fcntl (int fd, int op, ...  );

int co_ioctl(int fd, unsigned long op, ...);

int co_getsockopt(int sockfd, int level, int optname,
                    void* optval,
                    socklen_t * optlen);

int co_setsockopt(int sockfd, int level, int optname,
                    const void* optval,
                    socklen_t optlen);
}






// extern "C"
// {
//     // sleep 
//     using sleep_fun = unsigned int (*)(unsigned int seconds);
//     extern sleep_fun original_sleep;

//     using usleep_fun = unsigned int (*)(useconds_t usec);
//     extern usleep_fun original_usleep;

//     using nanosleep_fun = int (*)(const struct timespec* duration, struct timespec* rem);
//     extern nanosleep_fun original_nanosleep;

//     // socket
//     using socket_fun = int (*)(int domain, int type, int protocol);
//     extern socket_fun original_socket;

//     using accept_fun = int (*)(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
//     extern accept_fun original_accept;

//     using connect_fun = int (*)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
//     extern connect_fun original_connect;

//     // read
//     using read_fun = ssize_t (*)(int fd, void* buf, size_t count);
//     extern read_fun original_read;

//     using readv_fun = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt);
//     extern readv_fun original_readv;

//     using preadv_fun = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt, off_t offset);
//     extern preadv_fun original_preadv;

//     using preadv2_fun = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);
//     extern preadv2_fun original_preadv2;

//     using recv_fun = ssize_t (*)(int sockfd, void* buf, size_t len, int flags);
//     extern recv_fun original_recv;

//     using recvfrom_fun = ssize_t (*)(int sockfd, void* buf, size_t len, int flags, struct sockaddr *  src_addr, socklen_t *  addrlen);
//     extern recvfrom_fun original_recvfrom;

//     using recvmsg_fun = ssize_t (*)(int sockfd, struct msghdr *msg, int flags);
//     extern recvmsg_fun original_recvmsg;


//     // write
//     using write_fun = ssize_t (*)(int fd, const void* buf, size_t count);
//     extern write_fun original_write;

//     using writev_fun = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt);
//     extern writev_fun original_writev;

//     using pwritev_fun = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt, off_t offset);
//     extern pwritev_fun original_pwritev;

//     using pwritev2_fun = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);
//     extern pwritev2_fun original_pwritev2;

//     using send_fun = ssize_t (*)(int sockfd, const void* buf, size_t len, int flags);
//     extern send_fun original_send;

//     using sendto_fun = ssize_t (*)(int sockfd, const void* buf, size_t len, int flags,
//                       const struct sockaddr *dest_addr, socklen_t addrlen);
//     extern sendto_fun original_sendto;

//     using sendmsg_fun = ssize_t (*)(int sockfd, const struct msghdr *msg, int flags);
//     extern sendmsg_fun original_sendmsg;

//     // close
//     using close_fun = int (*)(int fd);
//     extern close_fun original_close;

//     // functional
//     using fcntl_fun = int (*)(int fd, int op, ...  );
//     extern fcntl_fun original_fcntl;

//     using ioctl_fun = int (*)(int fd, unsigned long op, ...);  
//     extern ioctl_fun original_ioctl;


//     // option
//     using getsockopt_fun = int (*)(int sockfd, int level, int optname,
//                       void* optval,
//                       socklen_t * optlen);
//     extern getsockopt_fun original_getsockopt;

//     using setsockopt_fun = int (*)(int sockfd, int level, int optname,
//                       const void* optval,
//                       socklen_t optlen);
//     extern setsockopt_fun original_setsockopt;



// }