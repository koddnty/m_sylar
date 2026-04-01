#include <iostream>
#include "DBPool/mysql.h"
#include "coroutine/corobase.h"


int main(void) {
    m_sylar::MySQLPoolManager dbPool(10, 20);
    if(-1 == dbPool.init("localhost", "koddnty", "73256", "KoddntyDB", 3306, 0)) {
        std::cout << "failed to init dbPool" << std::endl;
    }
    else {
        std::cout << "init dpPool successed" << std::endl;
    }
    return 0;
}