// analyse task conf
// parse parameters, build tasks(name, dir, cmd, args, perf_events)
// profile tasks
// output and finalize
#include <ostream>
#include <algorithm>
#include <unordered_map>
#include "Task.hpp"
#include "Config.hpp"

class ParallelProfiler {
public:
    ParallelProfiler(const SystemConfig& syscfg, const TaskConfig& taskcfg, const PlanConfig& plancfg, 
        std::ostream& output): m_syscfg(syscfg), m_taskcfg(taskcfg), m_plancfg(plancfg), m_output(output) {}
    ParallelProfiler(const ParallelProfiler&) = delete;
    ~ParallelProfiler() {
        for (const auto& proc : m_task) {
            delete proc;
        }
    }

    int profile();
    bool setOutput();
    bool addCPUSet(int cpu);
    bool addPlan(const std::string&);
private:
    // build task for each plan
    bool buildTask();

private:
    // static config
    const SystemConfig&                 m_syscfg;
    const TaskConfig&                   m_taskcfg;
    const PlanConfig&                   m_plancfg;
    std::ostream&                       m_output;
    std::vector<int>                    m_cpuset;
    std::vector<Plan*>                  m_plan;
    // running config
    std::vector<IProcess*>              m_task;
    std::unordered_map<pid_t, size_t>   m_pidmap;
};
