#include "until.h"
#include "log.h"
#include "fiber.h"


namespace m_sylar{

pid_t getThreadId (){
    return syscall(SYS_gettid);
}

std::string getThreadName (){
    // 获取当前线程，获取当前的threadName
    std::string threadName = "normalThread";
    auto thread = Thread::getThis();
    threadName = thread->getName();
    // if(thread){
    //     threadName = thread->getName();
    // }
    return threadName;
}

uint64_t getFiberId(){
    uint64_t fiberId = 0;
    auto fiber = Fiber::GetThisFiber();
    if(fiber){
        fiberId = fiber->getFiberId();
    }
    return fiberId;
}
   

void BackTrace(std::vector<std::string>& bt, int size, int skip){
    void** array = (void**) malloc(sizeof(void*) * size);
    size_t s = ::backtrace(array, size);
    char** traces = backtrace_symbols(array, s);
    if (traces == NULL) {
        M_SYLAR_LOG_ERROR(M_SYLAR_LOG_NAME("system")) << 
            "m_sylar::BackTrace traces is NULL";
        return;
    }
    
    for(size_t i = skip; i < s; i++) {
        bt.push_back(std::string(traces[i]));
    }

    free(traces);
    free(array);
}

std::string BackTraceToStr(int size, int skip, const std::string& prefix){
    std::vector<std::string> bt;
    BackTrace(bt, size, skip);
    std::stringstream ss;
    ss << prefix;
    for(const auto& it : bt){
        ss << it << std::endl;
    }
    return ss.str();
}

}

