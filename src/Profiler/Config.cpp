#include "Config.hpp"

//----------------------------------------------------------------------------//
// TaskAttribute

TaskAttribute::TaskAttribute(const Task& task): m_task(task) {}

//----------------------------------------------------------------------------//
// Plan

Plan::Plan(const TaskAttribute& task, const PerfAttribute& perf)
    : m_task(task), m_perf(perf) {}

bool
Plan::valid() const {
    const std::string& leader = m_perf.getLeader();
    uint64_t period = m_perf.getPeriod();
    uint64_t phaseStart = m_task.getPhaseBegin();
    uint64_t phaseEnd = m_task.getPhaseEnd();

    // Plan ID must exist.
    if (m_id.empty()) { return false; }

    // Check arguments for different plan type.
    switch (m_type) {
    case Type::DAEMON: return true;
    case Type::COUNT: return !leader.empty();
    case Type::SAMPLE_ALL: return !leader.empty() && period > 0;
    case Type::SAMPLE_PHASE: return !leader.empty() && period > 0 && phaseStart < phaseEnd;
    }

    return true;
}

//----------------------------------------------------------------------------//
