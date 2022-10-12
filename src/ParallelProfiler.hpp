#pragma once

#include <memory>
#include <ostream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include "Config.hpp"
#include "Process.hpp"
#include "PerfEventWrapper.hpp"

using Utils::Perf::Event;
using Utils::Perf::ChildEvent;
using EventPtr = std::shared_ptr<Event>;

struct IPerfProfiler {
    virtual int profile() = 0;
};

class ParallelProfiler: public IPerfProfiler {
protected:
    /**
     * Profile status
     * READY: All child processes have created correctly, the profiler should wait SIGTRAP to start all children.
     * INIT: Some sample plan doesn't match its phase condition, the profiler should wait for thoes child.
     * PROFILE: The profiler start profiling when all phase conditions are satisfied.
     * DONE: The profiler stop profiling and output when any child exit normally or meets its phase ending.
     * ABORT: The profiler just stop profiling when any child abort(terminated by signal), do nothing with output.
     */
    enum ProfileStatus { READY, INIT, PROFILE, DONE, ABORT, NR };

    struct RunningConfig {
        enum Status { RUN, STOP, DEAD };
        RunningConfig(const Plan&);

        pid_t           m_pid;
        int             m_cpu;
        const Plan&     m_plan;
        EventPtr        m_event;
        uint64_t        m_phaseno;
        Status          m_status;
        
    };
    using pidmap_t  = std::unordered_map<pid_t, RunningConfig>;
    using procset_t = std::unordered_set<pid_t>;

public:
    ParallelProfiler(std::ostream& output): m_output(output), m_status(ProfileStatus::DONE) {}
    ParallelProfiler(const ParallelProfiler&) = delete;
    ~ParallelProfiler() = default;
    void addCPUSet(int cpu);
    void addPlan(const Plan& plan);
    void setStatus(ProfileStatus);
    ProfileStatus getStatus() const;
    bool killAll();
    bool wakeupAll();
    virtual int profile() override;

private:
    // handle signal for child
    bool handleChild(pid_t);
    bool handleSignal(int);
    // check sudoer
    bool authCheck();
    // check validity for m_cpuset and m_plan
    bool argsCheck();

protected:
    // build running config for a plan
    bool buildRunningConfig(const Plan&);

protected:
    // static config
    std::ostream&               m_output;
    std::vector<int>            m_cpuset;
    std::vector<Plan>           m_plan;
    // running config
    pidmap_t                    m_pidmap;
    ProfileStatus               m_status;
    std::array<procset_t, NR>   m_pstatus;
};

ParallelProfiler::RunningConfig::RunningConfig(const Plan& plan)
    : m_pid(-1), m_cpu(-1), m_plan(plan), m_event(nullptr), m_phaseno(0), m_status(Status::RUN) {}

void
ParallelProfiler::addCPUSet(int cpu) {
    m_cpuset.push_back(cpu);
}

void
ParallelProfiler::addPlan(const Plan& plan) {
    m_plan.push_back(plan);
}

void
ParallelProfiler::setStatus(ProfileStatus status) {
    m_status = status;
}

ParallelProfiler::ProfileStatus
ParallelProfiler::getStatus() const {
    return m_status;
}
