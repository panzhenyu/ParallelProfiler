#include <ostream>
#include <algorithm>
#include <unordered_map>
#include "Config.hpp"
#include "Process.hpp"

struct IPerfProfiler {
    virtual int profile() = 0;
};

class ParallelProfiler: public IPerfProfiler {
public:
    enum ProcessStatus { READY, PHASEINIT, RUNNING, DEAD };
    
public:
    ParallelProfiler(std::ostream& output): m_output(output) {}
    ParallelProfiler(const ParallelProfiler&) = delete;
    ~ParallelProfiler() = default;

    virtual int profile() override;
    void addCPUSet(int cpu) { m_cpuset.push_back(cpu); }
    void addPlan(const Plan& plan) { m_plan.push_back(plan); }

private:
    // build task for each plan
    bool authCheck();
    bool argsCheck();
    bool buildTask();

private:
    // static config
    std::ostream&                       m_output;
    std::vector<int>                    m_cpuset;
    std::vector<Plan>                   m_plan;
    // running config
    std::vector<Process>                m_process;
    std::unordered_map<pid_t, size_t>   m_pidmap;
};
