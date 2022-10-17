#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unistd.h>

struct IValidConf {
    IValidConf() = default;
    IValidConf(const IValidConf&) = default;
    IValidConf(IValidConf&&) = default;
    IValidConf& operator=(const IValidConf&) = default;
    IValidConf& operator=(IValidConf&&) = default;
    ~IValidConf() = default;

    virtual bool valid() const = 0;
};

/**
 * Task Struct
 * When dir is empty, do not chdir.
 */
class Task {
public:
    Task(const Task&) = default;
    Task(Task&&) = default;
    Task& operator=(const Task&) = default;
    Task& operator=(Task&&) = default;
    ~Task() = default;

    //------------------------------------------------------------------------//
    // Setter

    Task& setID(const std::string& id);
    Task& setDir(const std::string& dir);
    Task& setCmd(const std::string& cmd);

    //------------------------------------------------------------------------//
    // Getter

    const std::string& getID() const;
    const std::string& getDir() const;
    const std::string& getCmd() const;

protected:
    friend class TaskFactory;
    Task() = default;

protected:
    std::string m_id;
    std::string m_dir;
    std::string m_cmd;
};

/**
 * TaskAttribute Struct
 */
class TaskAttribute {
public:
    static constexpr char*  SEPARATOR           = (char*)" ";
    static constexpr char*  ARG_PREFIX          = (char*)"$";
    static constexpr int    ARG_INDEX_BEGIN     = 1;

public:
    TaskAttribute(const TaskAttribute&) = default;
    TaskAttribute(TaskAttribute&&) = default;
    TaskAttribute& operator=(const TaskAttribute&) = default;
    TaskAttribute& operator=(TaskAttribute&&) = default;
    ~TaskAttribute() = default;

    //------------------------------------------------------------------------//
    // Setter

    TaskAttribute& setTask(const Task& task);
    TaskAttribute& setParam(const std::vector<std::string>& param);
    TaskAttribute& setRT(bool rt);
    TaskAttribute& setPinCPU(bool pincpu);
    TaskAttribute& setPhaseBegin(uint64_t begin);
    TaskAttribute& setPhaseEnd(uint64_t end);

    //------------------------------------------------------------------------//
    // Getter

    const Task& getTask() const;
    const std::vector<std::string>& getParam() const;
    bool isRT() const;
    bool needPinCPU() const;
    uint64_t getPhaseBegin() const;
    uint64_t getPhaseEnd() const;

protected:
    friend class TaskAttributeFactory;
    TaskAttribute(const Task& task);

protected:
    /**
     * @brief Task object, descrip basic info for a task.
     */
    Task                            m_task;

    /**
     * @brief Parameters for m_task.
     * m_param[k] is used to replace ${ARG_PREFIX}${ARG_INDEX_BEGIN+k} in m_task.m_cmd.
     * Say that we got a cmd "$1" and m_param={{"param1"}}, then we can generate cmd "param1".
     */
    std::vector<std::string>        m_param;

    /**
     * @brief This member deside whether we should build a RT task.
     */
    bool                            m_rt;

    /**
     * @brief This member deside whether we should bind task to a cpu.
     */
    bool                            m_pincpu;

    /**
     * @brief This member deside the phase of a task.
     * The meaning of m_phase is decided by plan.
     */
    std::pair<uint64_t, uint64_t>   m_phase;
};

/**
 * PerfAttribute Struct
 * We treat all perf event as a perf event group.
 */
class PerfAttribute {
public:
    PerfAttribute(const PerfAttribute&) = default;
    PerfAttribute(PerfAttribute&&) = default;
    PerfAttribute& operator=(const PerfAttribute&) = default;
    PerfAttribute& operator=(PerfAttribute&&) = default;
    ~PerfAttribute() = default;

    //------------------------------------------------------------------------//
    // Setter

    PerfAttribute& setLeader(const std::string& leader);
    PerfAttribute& setPeriod(uint64_t period);
    PerfAttribute& setEvents(const std::vector<std::string>& events);
    PerfAttribute& addEvents(const std::string& event);
    PerfAttribute& addEvents(const std::vector<std::string>& events);

    //------------------------------------------------------------------------//
    // Getter

    const std::string& getLeader() const;
    uint64_t getPeriod() const;
    const std::vector<std::string>& getEvents() const;

protected:
    friend class PerfAttributeFactory;
    PerfAttribute() = default;

protected:
    /**
     * @brief Leader of perf event group.
     */
    std::string                 m_leader;

    /**
     * @brief Sample period of perf event group leader.
     * A zero sample period means a count event.
     */
    uint64_t                    m_period;

    /**
     * @brief Group members.
     */
    std::vector<std::string>    m_events;
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
    Plan(const Plan&) = default;
    Plan(Plan&&) = default;
    Plan& operator=(const Plan&) = default;
    Plan& operator=(Plan&&) = default;
    ~Plan() = default;

    virtual bool valid() const override;
    bool samplePlan() const;
    bool perfPlan() const;

    //------------------------------------------------------------------------//
    // Setter

    Plan& setID(const std::string& id);
    Plan& setType(Plan::Type type);
    Plan& setTaskAttribute(const TaskAttribute& task);
    Plan& setPerfAttribute(const PerfAttribute& perf);

    //------------------------------------------------------------------------//
    // Getter
    const std::string& getID() const;
    Plan::Type getType() const;
    const TaskAttribute& getTaskAttribute() const;
    const PerfAttribute& getPerfAttribute() const;

protected:
    friend class PlanFactory;
    Plan(const TaskAttribute& task, const PerfAttribute& perf);

protected:
    //------------------------------------------------------------------------//
    // Plan Attribute

    /**
     * @brief Plan unique id.
     */
    std::string                 m_id;

    /**
     * @brief Plan type.
     */
    Plan::Type                  m_type;

    //------------------------------------------------------------------------//
    // Task Attribute

    /**
     * @brief Task attribute.
     */
    TaskAttribute               m_task;

    //------------------------------------------------------------------------//
    // Perf Attribute

    /**
     * @brief Perf attribute for perf events.
     */
    PerfAttribute               m_perf;

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

//----------------------------------------------------------------------------//
// Task

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

//----------------------------------------------------------------------------//
// TaskAttribute

TaskAttribute::TaskAttribute(const Task& task): m_task(task) {}

inline TaskAttribute&
TaskAttribute::setTask(const Task& task) {
    m_task = task;
    return *this;
}

inline TaskAttribute&
TaskAttribute::setParam(const std::vector<std::string>& param) {
    m_param = param;
    return *this;
}

inline TaskAttribute&
TaskAttribute::setRT(bool rt) {
    m_rt = rt;
    return *this;
}

inline TaskAttribute&
TaskAttribute::setPinCPU(bool pincpu) {
    m_pincpu = pincpu;
    return *this;
}

inline TaskAttribute&
TaskAttribute::setPhaseBegin(uint64_t begin) {
    m_phase.first = begin;
    return *this;
}

inline TaskAttribute&
TaskAttribute::setPhaseEnd(uint64_t end) {
    m_phase.second = end;
    return *this;
}

inline const Task&
TaskAttribute::getTask() const {
    return m_task;
}

inline const std::vector<std::string>&
TaskAttribute::getParam() const {
    return m_param;
}

inline bool
TaskAttribute::isRT() const {
    return m_rt;
}

inline bool
TaskAttribute::needPinCPU() const {
    return m_pincpu;
}

inline uint64_t
TaskAttribute::getPhaseBegin() const {
    return m_phase.first;
}

inline uint64_t
TaskAttribute::getPhaseEnd() const {
    return m_phase.second;
}

//----------------------------------------------------------------------------//
// PerfAttribute

inline PerfAttribute&
PerfAttribute::setLeader(const std::string& leader) {
    m_leader = leader;
    return *this;
}

inline PerfAttribute&
PerfAttribute::setPeriod(uint64_t period) {
    m_period = period;
    return *this;
}

inline PerfAttribute&
PerfAttribute::setEvents(const std::vector<std::string>& events) {
    m_events = events;
    return *this;
}

inline PerfAttribute&
PerfAttribute::addEvents(const std::string& event) {
    m_events.emplace_back(event);
    return *this;
}

inline PerfAttribute&
PerfAttribute::addEvents(const std::vector<std::string>& events) {
    m_events.insert(m_events.end(), events.begin(), events.end());
    return *this;
}

inline const std::string&
PerfAttribute::getLeader() const {
    return m_leader;
}

inline uint64_t
PerfAttribute::getPeriod() const {
    return m_period;
}

inline const std::vector<std::string>&
PerfAttribute::getEvents() const {
    return m_events;
}

//----------------------------------------------------------------------------//
// Plan

Plan::Plan(const TaskAttribute& task, const PerfAttribute& perf)
    : m_task(task), m_perf(perf) {}

inline bool
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
Plan::setType(Plan::Type type) {
    m_type = type;
    return *this;
}

inline Plan&
Plan::setTaskAttribute(const TaskAttribute& task) {
    m_task = task;
    return *this;
}

inline Plan&
Plan::setPerfAttribute(const PerfAttribute& perf) {
    m_perf = perf;
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

inline const TaskAttribute&
Plan::getTaskAttribute() const {
    return m_task;
}

inline const PerfAttribute&
Plan::getPerfAttribute() const {
    return m_perf;
}

//----------------------------------------------------------------------------//