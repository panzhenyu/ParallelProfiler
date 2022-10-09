#pragma once

#include <string>
#include <vector>
#include <unistd.h>
#include <functional>

class Process {
public:
    static pid_t start(const std::function<int()>& func);
    static pid_t start(const std::function<int()>& setup, const std::vector<std::string>& args);
};
