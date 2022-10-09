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
    Plan();
    Plan(const Plan&) = default;
    ~Plan() = default;
    virtual bool valid() const override;
    Plan& setID(const std::string& id);
    Plan& setTask(const Task& task);
    Plan& setParam(const std::vector<std::string>& param);
    Plan& setRT(bool rt);
    Plan& setPinCPU(bool pincpu);
    Plan& setEnablePhase(bool enablePhase);
    Plan& setPhase(const std::pair<int, int>& phase);
    Plan& serPerfLeader(const std::string leader);
    Plan& setPerfPeriod(uint64_t period);
    Plan& addPerfMember(const std::string& member);
    Plan& addPerfMember(const std::vector<std::string>& member);
    const std::string& getID() const;
    const Task& getTask() const;
    const std::vector<std::string>& getParam() const;
    bool isRT() const;
    bool needPinCPU() const;
    bool enbalePhase();
    std::pair<int, int> getPhase() const;
    const std::string& gerPerfLeader() const;
    uint64_t getPerfPeriod() const;
    const std::vector<std::string>& getPerfMember() const;

private:
    // plan identifiler
    std::string                 m_id;
    // task attribute, affect task build
    Task                        m_task;
    std::vector<std::string>    m_param;
    bool                        m_rt;
    bool                        m_pincpu;
    // perf attribute, affect task running
    bool                        m_enablePhase;
    std::pair<int, int>         m_phase;
    std::string                 m_leader;
    uint64_t                    m_period;
    std::vector<std::string>    m_member;
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
Plan::Plan(): m_enablePhase(false), m_rt(false), m_pincpu(false), m_period(0) {}

inline bool
Plan::valid() const {
    return !(m_id.empty() || m_leader.empty() || (m_enablePhase && 0 == m_period));
}

inline Plan&
Plan::setID(const std::string& id) {
    m_id = id;
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
Plan::setEnablePhase(bool enablePhase) {
    m_enablePhase = enablePhase;
    return *this;
}

inline Plan&
Plan::setPhase(const std::pair<int, int>& phase) {
    m_phase = phase;
    return *this;
}

inline Plan&
Plan::serPerfLeader(const std::string leader) {
    m_leader = leader;
    return *this;
}

inline Plan&
Plan::setPerfPeriod(uint64_t period) {
    m_period = period;
    return *this;
}

inline Plan&
Plan::addPerfMember(const std::string& member) {
    m_member.push_back(member);
    return *this;
}

inline Plan&
Plan::addPerfMember(const std::vector<std::string>& member) {
    m_member.insert(m_member.end(), member.begin(), member.end());
    return *this;
}

inline const std::string&
Plan::getID() const {
    return m_id;
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

inline bool
Plan::enbalePhase() {
    return m_enablePhase;
}

inline std::pair<int, int>
Plan::getPhase() const {
    return m_phase;
}

inline const std::string&
Plan::gerPerfLeader() const {
    return m_leader;
}

inline uint64_t
Plan::getPerfPeriod() const {
    return m_period;
}

inline const std::vector<std::string>&
Plan::getPerfMember() const {
    return m_member;
}
