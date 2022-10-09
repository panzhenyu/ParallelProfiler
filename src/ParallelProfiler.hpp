#pragma once

#include <memory>
#include <ostream>
#include <algorithm>
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
public:
    enum ProfileStatus { READY, PHASEINIT, RUNNING, DONE };

    struct RunningConfig {
        RunningConfig(const Plan&);

        pid_t       m_pid;
        int         m_cpu;
        const Plan& m_plan;
        EventPtr    m_event;
        uint64_t    m_phaseno;
    };
    
    using pidmap_t = std::unordered_map<pid_t, RunningConfig>;

public:
    ParallelProfiler(std::ostream& output): m_output(output) {}
    ParallelProfiler(const ParallelProfiler&) = delete;
    ~ParallelProfiler() = default;
    virtual int profile() override;
    void addCPUSet(int cpu);
    void addPlan(const Plan& plan);

private:
    // check sudoer
    bool authCheck();
    // check validity for m_cpuset and m_plan
    bool argsCheck();

protected:
    // add running config for a plan
    bool addRunningConfig(const Plan&);
    // collect perf data for every plan at each overflow
    bool collect();
    // kill all children
    void killAll();

protected:
    // static config
    std::ostream&                       m_output;
    std::vector<int>                    m_cpuset;
    std::vector<Plan>                   m_plan;
    // running config
    pidmap_t                            m_pidmap;
};

void
ParallelProfiler::addCPUSet(int cpu) {
    m_cpuset.push_back(cpu);
}

void
ParallelProfiler::addPlan(const Plan& plan) {
    m_plan.push_back(plan);
}

ParallelProfiler::RunningConfig::RunningConfig(const Plan& plan)
    : m_pid(-1), m_cpu(-1), m_plan(plan), m_event(nullptr), m_phaseno(0) {}
