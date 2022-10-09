#include <errno.h>
#include "Process.hpp"

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
    std::vector<char*> argv;

    if (0 == args.size()) {
        return false;
    }

    pid = fork();

    if (0 == pid) {
        // prepare args
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(0);
        // setup for this child process
        if (0 != (err=setup())) {
            exit(err);
        }
        // do exec
        if (-1 == execve(args[0].c_str(), argv.data(), {0})) {
            exit(-errno);
        }
    }
    
    return pid;
}
