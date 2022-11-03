#pragma once

#include "Config.hpp"

struct TaskFactory {
    static Task defaultTask();
    static Task buildTask(const std::string& id, const std::string& cmd);
    static Task buildTask(const std::string& id, const std::string& cmd, const std::string& dir);
};

struct TaskAttributeFactory {
    static TaskAttribute defaultTaskAttribute();
    static TaskAttribute normalTaskAttribute(const Task& task, const std::vector<std::string>& param, bool rt, bool pincpu);
    static TaskAttribute generalTaskAttribute(const Task& task, const std::vector<std::string>& param, 
        bool rt, bool pincpu, uint64_t phaseBegin, uint64_t phaseEnd);
};

struct PerfAttributeFactory {
    static PerfAttribute defaultPerfAttribute();
    static PerfAttribute simpleCountPerfAttribute(const std::string& leader);
    static PerfAttribute simpleSamplePerfAttribute(const std::string& leader, uint64_t period);
    static PerfAttribute generalPerfAttribute(const std::string& leader, uint64_t period, 
        const std::vector<std::string>& events);
};

struct PlanFactory {
    static Plan defaultPlan();
    static Plan daemonPlan(const std::string& id, const TaskAttribute& task);
    static Plan generalPlan(const std::string& id, Plan::Type type, const TaskAttribute& task, const PerfAttribute& perf);
};
