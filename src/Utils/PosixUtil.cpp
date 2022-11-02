#include <errno.h>
#include <fcntl.h>
#include "PosixUtil.hpp"

namespace Utils {
namespace Posix {

//----------------------------------------------------------------------------//
// Process

pid_t
Process::start(const std::function<int()>& func) {
    pid_t pid = fork();
    if (0 == pid) {
        exit(func());
    }
    return pid;
}

pid_t
Process::start(const std::function<int()>& setup, const std::vector<std::string>& args) {
    int err;
    pid_t pid;

    if (0 == args.size()) {
        return false;
    }

    pid = fork();

    if (0 == pid) {
        // setup for this child process
        if (0 != (err=setup())) {
            exit(err);
        }

        // do exec
        if (!Process::exec(args)) {
            exit(-errno);
        }
    }
    
    return pid;
}

bool
Process::exec(const std::vector<std::string>& args) {
    std::vector<char*> argv;

    // prepare args
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(0);

    // do exec
    return -1 != execve(args[0].c_str(), argv.data(), {0});
}

bool
Process::setCPUAffinity(pid_t pid, int cpu) {
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return 0 == sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset);
}

bool
Process::setFIFOProc(pid_t pid, int prio) {
    struct sched_param param;
    param.sched_priority = prio;
    return 0 == sched_setscheduler(pid, SCHED_FIFO, &param);
}

//----------------------------------------------------------------------------//
// File

bool
File::setFileOwner(int fd, pid_t owner) {
    return fcntl(fd, F_SETOWN, owner) != -1;
}

bool
File::setFileSignal(int fd, int signo) {
    return fcntl(fd, F_SETSIG, signo) != -1;
}

bool
File::enableSigalDrivenIO(int fd) {
    // Setup asynchronous notification on the file descriptor
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_ASYNC) != -1;
}

bool
File::disableSigalDrivenIO(int fd) {
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & (~O_ASYNC)) != -1;
}

//----------------------------------------------------------------------------//

} /* namespace Posix */
} /* namespace Utils */
