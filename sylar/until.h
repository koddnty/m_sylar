#pragma once

#include <thread>
#include <unistd.h>
#include <sys/syscall.h>
#include <cstdint>
#include <vector>
#include <execinfo.h>
#include <string>

namespace m_sylar{

pid_t getThreadId ();

uint32_t getFiberId();

void BackTrace(std::vector<std::string>& bt, int size, int skip = 1);

std::string BackTraceToStr(int size, int skip = 2, const std::string& prefix = "");

}