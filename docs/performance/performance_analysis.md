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


服务端4线程，3000并发情况，QPS约为3.7w/s。运行过程中没有发现明显内存泄漏与fd未回收情况


```she
(base) ╭─koddnty@koddnty-Legion-Y7000P-IRX9 ~/user/garbages 
╰─$ wrk -t4 -c3000 -d30s \
  -H "Content-Type: application/json" \
  -s rename.lua \
  http://127.0.0.1:8803/home/rename
Running 30s test @ http://127.0.0.1:8803/home/rename
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    57.79ms   39.43ms 339.02ms   76.18%
    Req/Sec     9.86k     3.68k   17.12k    65.51%
  1125522 requests in 30.11s, 97.68MB read
  Socket errors: connect 0, read 251852, write 0, timeout 0
Requests/sec:  37379.36
Transfer/sec:      3.24MB
```

