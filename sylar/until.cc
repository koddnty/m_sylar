#include "until.h"

namespace m_sylar{

pid_t getThreadId (){
    return syscall(SYS_gettid);
}


}

