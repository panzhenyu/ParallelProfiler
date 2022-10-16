#pragma once

#include <memory>
#include <ostream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include "Config.hpp"
#include "PerfEventWrapper.hpp"

using EventPtr = std::shared_ptr<Utils::Perf::Event>;

struct IPerfProfiler {
    virtual int profile() = 0;
};

class PerfProfiler: public IPerfProfiler {
public:
    using sample_t  = std::vector<uint64_t>;

    /**
     * @brief Profile a task, not implement yet.
     */
    virtual int profile() { return true; }

    /**
     * @brief Collect sample event by reading mmap buffer.
     */
    bool collect(EventPtr event, std::vector<sample_t>& data);

    /**
     * @brief Collect sample/count event by reading event fd.
     */
    bool collect(EventPtr event, sample_t& data);
};

class ParallelProfiler: public PerfProfiler {
public:
    /**
     * Profile status
     * READY: All child processes have created correctly, the profiler should wait SIGTRAP to start all children.
     * INIT: Some sample plan doesn't match its phase condition, the profiler should wait for thoes child.
     * PROFILE: The profiler start profiling when all phase conditions are satisfied. We all use this stage to sync children.
     * DONE: The profiler stop profiling and output when any child exit normally or meets its phase ending.
     * ABORT: The profiler just stop profiling when any child abort(terminated by signal), do nothing with output.
     */
    enum ProfileStatus { READY, INIT, PROFILE, DONE, ABORT, NR };

    struct RunningConfig {
        enum Status { RUN, STOP, DEAD };
        RunningConfig(const Plan&);

        pid_t                   m_pid;
        int                     m_cpu;
        const Plan&             m_plan;
        EventPtr                m_event;
        uint64_t                m_phaseno;
        Status                  m_status;
        std::vector<sample_t>   m_samples;
    };
    using pidmap_t  = std::unordered_map<pid_t, RunningConfig>;
    using procset_t = std::unordered_set<pid_t>;

public:
    ParallelProfiler(std::ostream& output): m_output(output), m_status(ProfileStatus::DONE) {}
    ParallelProfiler(const ParallelProfiler&) = delete;
    ~ParallelProfiler() = default;
    //------------------------------------------------------------------------//
    // Setter
    void addCPUSet(int cpu);
    void addCPUSet(const std::vector<int>& cpuset);
    void setCPUSet(const std::vector<int>& cpuset);
    void addPlan(const Plan& plan);
    void setStatus(ProfileStatus status);

    //------------------------------------------------------------------------//
    // Getter
    ProfileStatus getStatus() const;

    //------------------------------------------------------------------------//
    // Controller
    bool killChild(pid_t pid);
    bool killAll();
    bool wakeupChild(pid_t pid);
    bool wakeupAll();
    virtual int profile() override;

private:
    /**
     * @brief Handle signal for child.
     */
    bool handleChild(pid_t pid);

    /**
     * @brief Handle signal for main process.
     */
    bool handleSignal(int);

    /**
     * @brief Check sudoer rights.
     */
    bool authCheck();

    /**
     * @brief Check validity for m_cpuset and m_plan.
     */
    bool argsCheck();

    /**
     * @brief Use MMAP layout to update phaseno for child, do not save sample.
     */
    bool updatePhase(pid_t pid);

    /**
     * @brief Collect for perf plans by reading event fd and update phase for sample plan automatically.
     */
    bool collectAll();

    /**
     * @brief Prepare and step into INIT stage.
     */
    bool gotoINIT();

    /**
     * @brief Prepare and step into PROFILE stage.
     */
    bool gotoPROFILE();

    /**
     * @brief Prepare and step into DONE stage.
     */
    bool gotoDONE();

protected:
    /**
     * @brief Build running config for a plan
     */
    bool buildRunningConfig(const Plan&);

protected:
    //------------------------------------------------------------------------//
    // Settings

    /**
     * @brief Ostream object for output perf data.
     */
    std::ostream&               m_output;

    /**
     * @brief A set of cpuno can be used by child if a plan need to pin cpu.
     */
    std::vector<int>            m_cpuset;

    /**
     * @brief A set of plan for parallel profile.
     */
    std::vector<Plan>           m_plan;

    //------------------------------------------------------------------------//
    // Running config

    /**
     * @brief Map for running config, pid_t -> RunningConfig, each RunningConfig profile an process.
     */
    pidmap_t                    m_pidmap;

    /**
     * @brief Describe a profile stage.
     */
    ProfileStatus               m_status;

    /**
     * @brief Describe profiling status of each process. When m_pstatus[prof].count(pid) means pid has reached prof status.
     */
    std::array<procset_t, NR>   m_pstatus;
};

ParallelProfiler::RunningConfig::RunningConfig(const Plan& plan)
    : m_pid(-1), m_cpu(-1), m_plan(plan), m_event(nullptr), m_phaseno(0), m_status(Status::RUN) {}

inline void
ParallelProfiler::addCPUSet(int cpu) {
    m_cpuset.emplace_back(cpu);
}

inline void
ParallelProfiler::addCPUSet(const std::vector<int>& cpuset) {
    m_cpuset.insert(m_cpuset.end(), cpuset.begin(), cpuset.end());
}


inline void
ParallelProfiler::setCPUSet(const std::vector<int>& cpuset) {
    m_cpuset = cpuset;
}

inline void
ParallelProfiler::addPlan(const Plan& plan) {
    m_plan.emplace_back(plan);
}

inline void
ParallelProfiler::setStatus(ProfileStatus status) {
    m_status = status;
}

inline ParallelProfiler::ProfileStatus
ParallelProfiler::getStatus() const {
    return m_status;
}
