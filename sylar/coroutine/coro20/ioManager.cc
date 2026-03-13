#include "coroutine/coro20/ioManager.h"
#include "coroutine/coro20/hook.h"
#include "coroutine/coro20/fiber.h"
#include "coroutine/coro20/scheduler.h"
#include "basic/log.h"
#include "basic/macro.h"
#include "http/http.h"
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <shared_mutex>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace m_sylar
{
static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

}