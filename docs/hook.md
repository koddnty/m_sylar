# hook 文档

hook模块对linux大部分基础系统io调用进行了异步管理，达到协程效果。

## hook基础概述

具体hook函数有:

主线程无法启用hook功能

## hook接口

### is_hook_enable

**函数签名**  

```cpp
    bool is_hook_enable();
```

**函数描述**  

返回当前线程是否启用hook功能
请注意，请不要为框架中的线程池线程启用此功能，**不要**将``hook.cc``中的``t_hook_enable``默认值改为``true``，这将导致巨大错误。

**参数**  

无

**返回值**  

启用返回``true``,未启用返回``false``

### set_hook_state

**函数签名**  

```cpp
    void set_hook_state(bool flags);
```

**函数描述**  

设置当前线程hook状态

**参数**  

``flags``表示是否启用hook

**返回值**  

无  

## 框架HOOK管理

框架启动时,将会为线程池线程自动启用hook功能。使用ioManager创建一个IO管理类，向内添加的任务中上述提供的函数启用hook功能。
