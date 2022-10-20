#include "ConfigFactory.hpp"

//----------------------------------------------------------------------------//
// TaskFactory

Task
TaskFactory::defaultTask() {
    return Task();
}

Task
TaskFactory::buildTask(const std::string& id, const std::string& cmd) {
    return buildTask(id, cmd, ".");
}

Task
TaskFactory::buildTask(const std::string& id, const std::string& cmd, const std::string& dir) {
    return defaultTask().setID(id).setDir(dir).setCmd(cmd);
}

//----------------------------------------------------------------------------//
// TaskAttributeFactory

TaskAttribute
TaskAttributeFactory::defaultTaskAttribute() {
    return normalTaskAttribute(TaskFactory::defaultTask(), std::vector<std::string>(), false, false);
}

TaskAttribute
TaskAttributeFactory::normalTaskAttribute(const Task& task, const std::vector<std::string>& param, bool rt, bool pincpu) {
    return generalTaskAttribute(task, param, rt, pincpu, 0, 0);
}
TaskAttribute
TaskAttributeFactory::generalTaskAttribute(const Task& task, const std::vector<std::string>& param, 
bool rt, bool pincpu, uint64_t phaseBegin, uint64_t phaseEnd) {
    return TaskAttribute(task).setParam(param).setRT(rt).setPinCPU(pincpu).setPhaseBegin(phaseBegin).setPhaseEnd(phaseEnd);
}

//----------------------------------------------------------------------------//
// PerfAttributeFactory

PerfAttribute
PerfAttributeFactory::defaultPerfAttribute() {
    return simpleCountPerfAttribute(std::string());
}

PerfAttribute
PerfAttributeFactory::simpleCountPerfAttribute(const std::string& leader) {
    return simpleSamplePerfAttribute(leader, 0);
}

PerfAttribute
PerfAttributeFactory::simpleSamplePerfAttribute(const std::string& leader, uint64_t period) {
    return generalPerfAttribute(leader, period, std::vector<std::string>());
}

PerfAttribute
PerfAttributeFactory::generalPerfAttribute(const std::string& leader, uint64_t period, 
const std::vector<std::string>& events) {
    return PerfAttribute().setLeader(leader).setPeriod(period).setEvents(events);
}

//----------------------------------------------------------------------------//
// PlanFactory

Plan
PlanFactory::defaultPlan(const std::string& id, Plan::Type type) {
    return generalPlan(id, type, TaskAttributeFactory::defaultTaskAttribute(), 
        PerfAttributeFactory::defaultPerfAttribute());
}

Plan
PlanFactory::daemonPlan(const std::string& id, const TaskAttribute& task) {
    return generalPlan(id, Plan::Type::DAEMON, task, PerfAttributeFactory::defaultPerfAttribute());
}

Plan
PlanFactory::generalPlan(const std::string& id, Plan::Type type, const TaskAttribute& task, const PerfAttribute& perf) {
    return Plan(task, perf).setID(id).setType(type);
}

//----------------------------------------------------------------------------//
