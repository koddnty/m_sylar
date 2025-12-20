# ioManager 接口

ioManager是scheduler的一个子类，一般情况下所有任务都通过iomanager执行，scheduler不具备完整执行任务的能力(存在缺陷)。

int IOManager::addEvent(int fd, Event event, std::function<void()> cb_func)
