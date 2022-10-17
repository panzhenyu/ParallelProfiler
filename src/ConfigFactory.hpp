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
    static Plan defaultPlan(const std::string& id, Plan::Type type);
    static Plan daemonPlan(const std::string& id, const TaskAttribute& task);
    static Plan generalPlan(const std::string& id, Plan::Type type, const TaskAttribute& task, const PerfAttribute& perf);
};

//----------------------------------------------------------------------------//
// TaskFactory

inline Task
TaskFactory::defaultTask() {
    return Task();
}

inline Task
TaskFactory::buildTask(const std::string& id, const std::string& cmd) {
    return buildTask(id, cmd, ".");
}

inline Task
TaskFactory::buildTask(const std::string& id, const std::string& cmd, const std::string& dir) {
    return defaultTask().setID(id).setDir(dir).setCmd(cmd);
}

//----------------------------------------------------------------------------//
// TaskAttributeFactory

inline TaskAttribute
TaskAttributeFactory::defaultTaskAttribute() {
    return normalTaskAttribute(TaskFactory::defaultTask(), std::vector<std::string>(), false, false);
}

inline TaskAttribute
TaskAttributeFactory::normalTaskAttribute(const Task& task, const std::vector<std::string>& param, bool rt, bool pincpu) {
    return generalTaskAttribute(task, param, rt, pincpu, 0, 0);
}
inline TaskAttribute
TaskAttributeFactory::generalTaskAttribute(const Task& task, const std::vector<std::string>& param, 
bool rt, bool pincpu, uint64_t phaseBegin, uint64_t phaseEnd) {
    return TaskAttribute(task).setParam(param).setRT(rt).setPinCPU(pincpu).setPhaseBegin(phaseBegin).setPhaseEnd(phaseEnd);
}

//----------------------------------------------------------------------------//
// PerfAttributeFactory

inline PerfAttribute
PerfAttributeFactory::defaultPerfAttribute() {
    return simpleCountPerfAttribute(std::string());
}

inline PerfAttribute
PerfAttributeFactory::simpleCountPerfAttribute(const std::string& leader) {
    return simpleSamplePerfAttribute(leader, 0);
}

inline PerfAttribute
PerfAttributeFactory::simpleSamplePerfAttribute(const std::string& leader, uint64_t period) {
    return generalPerfAttribute(leader, period, std::vector<std::string>());
}

inline PerfAttribute
PerfAttributeFactory::generalPerfAttribute(const std::string& leader, uint64_t period, 
const std::vector<std::string>& events) {
    return PerfAttribute().setLeader(leader).setPeriod(period).setEvents(events);
}

//----------------------------------------------------------------------------//
// PlanFactory

inline Plan
PlanFactory::defaultPlan(const std::string& id, Plan::Type type) {
    return generalPlan(id, type, TaskAttributeFactory::defaultTaskAttribute(), 
        PerfAttributeFactory::defaultPerfAttribute());
}

inline Plan
PlanFactory::daemonPlan(const std::string& id, const TaskAttribute& task) {
    return generalPlan(id, Plan::Type::DAEMON, task, PerfAttributeFactory::defaultPerfAttribute());
}

inline Plan
PlanFactory::generalPlan(const std::string& id, Plan::Type type, const TaskAttribute& task, const PerfAttribute& perf) {
    return Plan(task, perf).setID(id).setType(type);
}

//----------------------------------------------------------------------------//