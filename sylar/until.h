#pragma once

#include <thread>
#include <unistd.h>
#include <sys/syscall.h>
#include <cstdint>
#include <vector>
#include <execinfo.h>
#include <filesystem>
#include <string>

namespace m_sylar{

pid_t getThreadId ();
std::string getThreadName ();
uint64_t getFiberId();

void BackTrace(std::vector<std::string>& bt, int size, int skip = 1);

std::string BackTraceToStr(int size, int skip = 2, const std::string& prefix = "");



inline std::string getExecutableDir() {
    // 获取可执行文件所在目录
    std::filesystem::path exe_path = std::filesystem::read_symlink("/proc/self/exe");
    return exe_path.parent_path().parent_path().string();
}
}