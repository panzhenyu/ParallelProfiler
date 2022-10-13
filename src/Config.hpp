#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unistd.h>

struct IValidConf {
    virtual bool valid() const = 0;
};

/**
 * Task Struct
 * When dir is empty, do not chdir.
 */
class Task: public IValidConf {
public:
    static constexpr char*  SEPARATOR           = (char*)" ";
    static constexpr char*  ARG_PREFIX          = (char*)"$";
    static constexpr int    ARG_INDEX_BEGIN     = 1;
public:
    Task() = default;
    Task(const std::string& id, const std::string& cmd);
    Task(const std::string& id, const std::string& dir, const std::string& cmd);
    Task(const Task&) = default;
    ~Task() = default;
    virtual bool valid() const override;
    Task& setID(const std::string& id);
    Task& setDir(const std::string& dir);
    Task& setCmd(const std::string& cmd);
    const std::string& getID() const;
    const std::string& getDir() const;
    const std::string& getCmd() const;

private:
    std::string m_id;
    std::string m_dir;
    std::string m_cmd;
};

/**
 * Plan Struct
 * Size of m_param must match m_task.m_cmd.
 * Trigger count mode for this plan When m_period is 0.
 * Empty m_leader and m_event means a daemon plan.
 */
class Plan: public IValidConf {
public:
    /**
     * Plan Type
     * DAEMON: Ignore all perf configuration.
     * COUNT: Enable m_leader(Required), m_events(Optional), ignore m_period, m_phase.
     * SAMPLE: Enable m_leader(Required), m_events(Optional), m_period(must > 0), ignore m_phase.
     * PHASE: Enable m_leader(Required), m_events(Optional), m_period(must > 0), m_phase(start < end).
     */
    enum Type { DAEMON, COUNT, SAMPLE_ALL, SAMPLE_PHASE };
public:
    Plan(const std::string& id, Type type, const Task& task);
    Plan(const Plan&) = default;
    ~Plan() = default;
    virtual bool valid() const override;
    bool samplePlan() const;
    bool perfPlan() const;
    Plan& setID(const std::string& id);
    Plan& setType(Type type);
    Plan& setTask(const Task& task);
    Plan& setParam(const std::vector<std::string>& param);
    Plan& setRT(bool rt);
    Plan& setPinCPU(bool pincpu);
    Plan& setPhase(const std::pair<int, int>& phase);
    Plan& setPerfLeader(const std::string leader);
    Plan& setPerfPeriod(uint64_t period);
    Plan& add2PerfEvents(const std::string& event);
    Plan& add2PerfEvents(const std::vector<std::string>& events);
    const std::string& getID() const;
    Type getType() const;
    const Task& getTask() const;
    const std::vector<std::string>& getParam() const;
    bool isRT() const;
    bool needPinCPU() const;
    std::pair<int, int> getPhase() const;
    const std::string& getPerfLeader() const;
    uint64_t getPerfPeriod() const;
    const std::vector<std::string>& getPerfEvents() const;

private:
    // plan attribute
    std::string                 m_id;
    Type                        m_type;
    // task attribute, affect task build
    Task                        m_task;
    std::vector<std::string>    m_param;
    bool                        m_rt;
    bool                        m_pincpu;
    // perf attribute, affect task running
    std::pair<int, int>         m_phase;
    std::string                 m_leader;
    uint64_t                    m_period;
    std::vector<std::string>    m_events;
};

/**
 * SystemConfig
 * collect system config such as cpu num
 */
class SystemConfig: public IValidConf {
public:
    virtual bool valid() const override;

};

/**
 * TaskConfig
 * Parse task config file, provide task object
 */
class TaskConfig: public IValidConf {
public:
    virtual bool valid() const override;
};

/**
 * PlanConfig
 * Parse plan config file, provide plan object
 */
class PlanConfig: public IValidConf {
public:
    virtual bool valid() const override;
};

/* Task: inline implementation */

Task::Task(const std::string& id, const std::string& cmd): Task(id, ".", cmd) {}

Task::Task(const std::string& id, const std::string& dir, const std::string& cmd)
    : m_id(id), m_dir(dir), m_cmd(cmd) {}
    
inline bool
Task::valid() const  {
    return true;
}

inline Task&
Task::setID(const std::string& id) {
    m_id = id;
    return *this;
}

inline Task&
Task::setDir(const std::string& dir) {
    m_dir = dir;
    return *this;
}

inline Task&
Task::setCmd(const std::string& cmd) {
    m_cmd = cmd;
    return *this;
}

inline const std::string&
Task::getID() const {
    return m_id;
}

inline const std::string&
Task::getDir() const {
    return m_dir;
}

inline const std::string&
Task::getCmd() const {
    return m_cmd;
}

/* plan: inline implementation */
Plan::Plan(const std::string& id, Type type, const Task& task)
    : m_id(id), m_type(type), m_task(task), m_rt(false), m_pincpu(false), m_period(0), m_phase({0, 0}) {}

inline bool
Plan::valid() const {
    // the needed attr is invalid
    if (m_id.empty() || !m_task.valid()) { return false; }

    // valid args for each type
    switch (m_type) {
    case Type::DAEMON: return true;
    case Type::COUNT: return !m_leader.empty();
    case Type::SAMPLE_ALL: return !m_leader.empty() && m_period > 0;
    case Type::SAMPLE_PHASE: return !m_leader.empty() && m_period > 0 && m_phase.first < m_phase.second;
    }

    return true;
}

inline bool
Plan::samplePlan() const {
    return getType() >= Type::SAMPLE_ALL;
}

inline bool
Plan::perfPlan() const {
    return getType() >= Type::COUNT;
}

inline Plan&
Plan::setID(const std::string& id) {
    m_id = id;
    return *this;
}

inline Plan&
Plan::setType(Type type) {
    m_type = type;
    return *this;
}

inline Plan&
Plan::setTask(const Task& task) {
    m_task = task;
    return *this;
}

inline Plan&
Plan::setParam(const std::vector<std::string>& param) {
    m_param = param;
    return *this;
}

inline Plan&
Plan::setRT(bool rt) {
    m_rt = rt;
    return *this;
}

inline Plan&
Plan::setPinCPU(bool pincpu) {
    m_pincpu = pincpu;
    return *this;
}

inline Plan&
Plan::setPhase(const std::pair<int, int>& phase) {
    m_phase = phase;
    return *this;
}

inline Plan&
Plan::setPerfLeader(const std::string leader) {
    m_leader = leader;
    return *this;
}

inline Plan&
Plan::setPerfPeriod(uint64_t period) {
    m_period = period;
    return *this;
}

inline Plan&
Plan::add2PerfEvents(const std::string& event) {
    m_events.push_back(event);
    return *this;
}

inline Plan&
Plan::add2PerfEvents(const std::vector<std::string>& events) {
    m_events.insert(m_events.end(), events.begin(), events.end());
    return *this;
}

inline const std::string&
Plan::getID() const {
    return m_id;
}

inline Plan::Type
Plan::getType() const {
    return m_type;
}

inline const Task&
Plan::getTask() const {
    return m_task;
}

inline const std::vector<std::string>&
Plan::getParam() const {
    return m_param;
}

inline bool
Plan::isRT() const {
    return m_rt;
}

inline bool
Plan::needPinCPU() const {
    return m_pincpu;
}

inline std::pair<int, int>
Plan::getPhase() const {
    return m_phase;
}

inline const std::string&
Plan::getPerfLeader() const {
    return m_leader;
}

inline uint64_t
Plan::getPerfPeriod() const {
    return m_period;
}

inline const std::vector<std::string>&
Plan::getPerfEvents() const {
    return m_events;
}
