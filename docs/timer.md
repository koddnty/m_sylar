# Timer 实现方式与接口

## 实现方式

使用linux提供的timerFd和已实现的[``ioManager``](./ioManager.md)来实现定时任务的唤醒与执行。由于是异步任务执行，在一定情况下可能任务执行时间与循环时间间隔不一致。

## 接口

### addTimer

**函数描述**  

创建一个新的定时器任务并将其注册到iom(io任务管理类)中。并返回新创建的任务的`timerFd`用于定时器任务的取消。

**函数签名**  

```cpp
int addTimer(uint64_t intervalTime, bool is_cycle, std::shared_ptr<TimeManager> manager,std::function<void()> main_cb)
```

- ``intervalTime`` 循环时间，以微秒计（1:1000000)  

- ``is_cycle`` 是否循环计时

- ``manager`` [iomanager](./ioManager.md)创建的io任务管理类  

- ``main_cb`` 计时器到时间后的执行函数。需要的函数签名：``std::function<void()>``

**返回值**  

返回创建的定时器任务的`timerFd`。

### addConditionTimer

**函数描述**  

创建一个新的**条件**定时器任务并将其注册到iom(io任务管理类)中。并返回新创建的任务的`timerFd`用于定时器任务的取消。计时完成后将会根据条件分别执行不同函数。

**函数签名**  

```cpp
int addConditionTimer(uint64_t intervalTime, bool is_cycle, 
    std::shared_ptr<TimeManager> manager,
    std::function<void()> main_cb,
    std::function<bool()> condition,
    std::function<void()> condition_cb);
```

- 前4个参数与``addTimer``相同

- ``condition`` 计时器到时间后将优先进行condition的判断，如果返回true，则继续执行main_cb, 如果返回false，则执行condition_cb。需要的函数签名：``std::function<void()>``

- ``condition_cb`` 计时器到时间后condition返回值为false执行的函数, 需要的函数签名：``std::function<void()>``

**返回值**  

返回创建的定时器任务的`timerFd`。

### cancelTimer

**函数描述**  

取消一个定时器任务

**函数签名**  

```cpp
void cancelTimer(Timer::ptr timer); 
```

- 待取消的timer智能指针(现无法获得)

```cpp
void cancelTimer(int timerFd);
```

- 待取消的定时器对应的``timerFd``

**返回值**  

无
