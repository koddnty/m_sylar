#include "lab/iomanager.h"
#include "basic/log.h"


void func()
{
    std::cout << "say goodbye" << std::endl;
}


int main(void)
{
    m_sylar::IOManagerCoro20 iom("main", 1);
    iom.schedule(func);
    iom.stop();
    return 0;
}