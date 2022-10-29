#pragma once

#include <unordered_set>
#include <unordered_map>
#include "PerfProfiler.hpp"

class ParallelProfiler: public PerfProfiler {
public:
    static constexpr int OVERFLOW_SIG = 64;
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
    using result_t  = std::map<std::string, uint64_t>;

private:
    /**
     * @brief   Setup function for Utils::Posix::Process::start.
     *          We are in the child process, just configure it.
     * 
     * @param[in] task A Task object contains initialize information for a static task.
     * 
     * @returns 0 if setup succeed, otherwise returns -errno. 
     */
    static int setupSyncTask(const Task& task);

    /**
     * @brief   Create signalfd to handle these signal.
     *          We must ignore SIGCHLD before start process, or SIGCHLD may be handled by default handler.
     *          Note that the blocked signals will be derived by children in this way.
     * 
     * SIGINT: Send SIGKILL for all children, terminate profiler without output.
     * SIGIO: Ignore SIGIO, cause signal driven IO send signal to the group.
     * SIGCHLD: Do nothing with this signal, whose default behavior is ignore.
     */
    static int createSignalFD();

public:
    ParallelProfiler(std::ostream& log);
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
    // Info reporter

    std::string showCPUSet() const;
    std::string showPlan() const;

    //------------------------------------------------------------------------//
    // Getter

    ProfileStatus getStatus() const;
    const std::map<std::string, ParallelProfiler::result_t>& getLastResult() const;

    //------------------------------------------------------------------------//
    // Controller

    bool killChild(pid_t pid);
    bool killAll();
    bool wakeupChild(pid_t pid, int signo);
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
     * @brief A set of cpuno can be used by child if a plan need to pin cpu.
     */
    std::vector<int>                m_cpuset;

    /**
     * @brief A set of plan for parallel profile.
     */
    std::vector<Plan>               m_plan;

    //------------------------------------------------------------------------//
    // Running config

    /**
     * @brief Map for running config, pid_t -> RunningConfig, each RunningConfig profile an process.
     */
    pidmap_t                        m_pidmap;

    /**
     * @brief Describe a profile stage.
     */
    ProfileStatus                   m_status;

    /**
     * @brief Describe profiling status of each process. When m_pstatus[prof].count(pid) means pid has reached prof status.
     */
    std::array<procset_t, NR>       m_pstatus;

    /**
     * @brief Store result for each plan.
     * 
     * Result format:
     *      m_result = {planid: result_t[, planid: result_t]}
     *      result_t = {event: count[, event: count]}
     */
    std::map<std::string, result_t> m_result;

    //------------------------------------------------------------------------//
};

//----------------------------------------------------------------------------//
// ParallelProfiler

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

inline std::string
ParallelProfiler::showCPUSet() const {
    std::string out;
    for (auto cpu : m_cpuset) {
        out += std::to_string(cpu);
        out += ",";
    }
    if (!out.empty()) { out.pop_back(); }
    return out;
}

inline std::string
ParallelProfiler::showPlan() const {
    std::string out;
    for (const auto& plan : m_plan) {
        out += plan.getID();
        out += ",";
    }
    if (!out.empty()) { out.pop_back(); }
    return out;
}

inline ParallelProfiler::ProfileStatus
ParallelProfiler::getStatus() const {
    return m_status;
}


inline const std::map<std::string, ParallelProfiler::result_t>&
ParallelProfiler::getLastResult() const {
    return m_result;
}

//----------------------------------------------------------------------------//
