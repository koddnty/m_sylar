# ioManager描述

ioManager是scheduler的一个子类，一般情况下所有任务都通过iomanager执行，scheduler不具备完整执行任务的能力(存在缺陷)。

## 主要功能

创建线程池，使用epoll对普通任务和io任务进行管理。

## 接口与描述

对于iomanager来说，主要使用的类为ioManager类，每个ioManager对应一个创建的线程池并通过提供的方法添加io任务或其他任务，同时线程池中线程默认启用[hook](./hook.md)，依赖协程的调度。

### tickle

**函数定义**  

```cpp
void tickle() override;
```

**函数描述**  

对scheduler类的tickle虚函数重写函数，主要实现对其他线程的唤醒。  
调用tickle时，将尝试唤醒阻塞在epoll_wait的线程。

**函数参数**  

无

**返回值**  

无

### stopping

此函数直接调用scheduler的stopping函数。

### idle

**函数定义**  

```cpp
void idle() override;
```

**函数描述**  

任务队列为空且无io任务时，线程将创建子协程运行至idle函数并陷入epoll_wait的阻塞状态。  
当线程被唤醒，会先对简单的io任务进行处理，即运行io任务的回调函数。  
io处理完毕后，线程将使用协程功能交出当前协程控制权，主协程将重新创建子协程重新运行[``run``](./scheudler.md#run)函数组成循环  
在running中协程将会检测任务队列并执行任务。  

由于处理io回调任务的为子协程，若在这个回调任务创建新的协程将导致原协程上下文丢失，但应该不会出现巨大问题。

**函数参数**  

无

**返回值**  

无

## 主要类描述

### ioManager

``ioManager``只有一个主要类``IOManager``, 继承自[``scheduler``](./scheudler.md#scheduler)，其中还定义时间枚举类型[``Evnet``](#iomanagerevent)和其他两个事件类[``FdContext``](#iomanagerfdcontext)、[``EventContext``](#iomanagerfdcontexteventcontext)。

该类负责对能够被epoll管理的io及其对应任务的管理，通过为fd的一个Event添加事件来调用其回调函数达到IO多路复用的效果。

#### 主要成员变量

private:

- int m_epollFd;                                    // epoll文件描述符
- int m_eventFd;                                    // event通信句柄
- std::atomic<size_t> m_pendding_event_count;       // 事件计数
- std::vector<FdContext*> m_fdContexts;             // 管理的fdConContext

### ioManager::FdContext

``FdContext``类是

### ioManager::FdContext::EventContext

### ioManager::Event

io事件类型，分三种，分别是:

- 可读 ``READ``
- 可写 ``WRITE``
- 无 ``NONE``

int IOManager::addEvent(int fd, Event event, std::function<void()> cb_func)

## 模块依赖



**函数定义**  

```cpp

```

**函数描述**  

**函数参数**  

**返回值**  