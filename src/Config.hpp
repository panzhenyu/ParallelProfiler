#include <string>
#include <vector>
#include <cstdint>
#include <unistd.h>

/**
 * Task Struct
 * When dir is empty, do not chdir.
 */
struct Task {
    std::string m_id;
    std::string m_dir;
    std::string m_cmd;
};

/**
 * Plan Struct
 * Size of m_param must match m_task.m_cmd.
 * Size of m_phase must be 2.
 * Trigger count mode for this plan When m_period is 0.
 * Empty m_leader and m_event means a daemon plan.
 */
struct Plan {
    enum PlanType { DAEMON, PERF };
    // plan identifiler
    std::string                 m_id;
    // task attribute
    const Task&                 m_task;
    std::vector<std::string>    m_param;
    std::pair<int, int>         m_phase;
    bool                        m_rt;
    // sample attribute
    std::string                 m_leader;
    uint64_t                    m_period;
    std::vector<std::string>    m_event;
};

/**
 * SystemConfig
 * collect system config such as cpu num
 */
class SystemConfig {

};

/**
 * TaskConfig
 * Parse task config file, provide task object
 */
class TaskConfig {

};

/**
 * PlanConfig
 * Parse plan config file, provide plan object
 */
class PlanConfig {

};
