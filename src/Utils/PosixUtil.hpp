#pragma once

#include <string>
#include <vector>
#include <unistd.h>
#include <functional>

namespace Utils {
namespace Posix {

class Process {
public:
    static pid_t start(const std::function<int()>& func);
    static pid_t start(const std::function<int()>& setup, const std::vector<std::string>& args);
    static bool exec(const std::vector<std::string>& args);
    static bool setCPUAffinity(pid_t pid, int cpu);
    static bool setFIFOProc(pid_t pid, int prio);
};

class File {
public:
    static bool setFileOwner(int fd, pid_t owner);
    static bool setFileSignal(int fd, int signo);
    static bool enableSigalDrivenIO(int fd);
    static bool disableSigalDrivenIO(int fd);
};

} /* namespace Posix */
} /* namespace Utils */
