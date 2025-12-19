# ioManager 接口

int IOManager::addEvent(int fd, Event event, std::function<void()> cb_func)
