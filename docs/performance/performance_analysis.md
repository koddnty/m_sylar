# 性能分析

(待完善，没空啊QAQ)

## 简单任务，协程调度性能

### 测试代码:

``` cpp		
#include "coroutine/corobase.h"
#include "basic/log.h"
#include <atomic>
#include <ostream>


std::atomic<int> count = 0;

m_sylar::Task<void, m_sylar::TaskBeginExecuter> task()
{
    m_sylar::IOManager::getInstance()->schedule(m_sylar::TaskCoro20::create_coro(task));
    count++;
    co_return;
}

void test_coroutine_task()
{
    m_sylar::IOManager iom("test_coroutine", 4);
    std::cout << "schedule task ..." << std::endl;

    int total = 100000;
    for(int i = 0; i < total; i++)
    {
        iom.schedule(m_sylar::TaskCoro20::create_coro(task));
    }

    std::cout << "schedule task finished" << std::endl;
   	int count_begin = count;
    iom.getTaskCount();

    int time = 120;
    sleep(time);
    iom.autoStop();
    std::cout << "have runed " << count - count_begin << " in " << time << " sec" << std::endl;
}

int main(void)
{
    test_coroutine_task();
    return 0;
}
```

![image-20260322205958693](/home/koddnty/.config/Typora/typora-user-images/image-20260322205958693.png)

### 结果

```shell	
╭─koddnty@koddnty ~/user/projects/sylar/m_sylar/m_sylar/build ‹refactorToCppCoro●› 
╰─$ ./../bin/scheduleCoro20_test 
schedule task ...
schedule task finished
have runed 192999456 in 120 sec
```

120s内共调度192999456个简单任务，平均约120w/s，各个线程占用率基本吃满并没有发现内存泄漏情况。

## http简单解析与响应

代码详见[代码](./../../tests/httpServer_test.cc)

执行过程中4线程，发现并发仅有1.7wQPS（无日志情况下）且cpu占用率较低，需要进行进一步优化，但执行过程中并未发现内存泄漏与内存错误等问题。

10分钟wrk测试中较为稳定，wrk压力测试结果如下,连接数1000,时600s:

```she
╭─koddnty@koddnty-Legion-Y7000P-IRX9 ~/user/garbages 
╰─$ wrk -t8 -c1000 -d600s \
  -H "Content-Type: application/json" \
  -s rename.lua \
  http://127.0.0.1:8803/home/rename
Running 10m test @ http://127.0.0.1:8803/home/rename
  8 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    75.41ms   30.69ms 336.40ms   66.21%
    Req/Sec     1.66k   785.62     7.48k    82.91%
  7926384 requests in 10.00m, 687.89MB read
Requests/sec:  13208.51
Transfer/sec:      1.15MB
```

