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
    Task(const std::string& id, const std::string& cmd): Task(id, ".", cmd) {}
    Task(const std::string& id, const std::string& dir, const std::string& cmd)
        : m_id(id), m_dir(dir), m_cmd(cmd) {}
    Task(const Task&) = default;
    ~Task() = default;
    
    virtual bool valid() const override { return true; }
    Task& setID(const std::string& id) { m_id = id; return *this; }
    Task& setDir(const std::string& dir) { m_dir = dir; return *this; }
    Task& setCmd(const std::string& cmd) { m_cmd = cmd; return *this; }
    const std::string& getID() const { return m_id; }
    const std::string& getDir() const { return m_dir; }
    const std::string& getCmd() const { return m_cmd; }
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
    Plan(): m_enablePhase(false), m_rt(false), m_pincpu(false), m_period(0) {}
    Plan(const Plan&) = default;
    ~Plan() = default;

    virtual bool valid() const override {
        return !(m_id.empty() || m_leader.empty() || (m_enablePhase && 0 == m_period));
    }

    Plan& setID(const std::string& id) { m_id = id; return *this; }

    Plan& setTask(const Task& task) { m_task = task; return *this; }
    Plan& setParam(const std::vector<std::string>& param) { m_param = param; return *this; }
    Plan& setRT(bool rt) { m_rt = rt; return *this; }
    Plan& setPinCPU(bool pincpu) { m_pincpu = pincpu; return *this; }

    Plan& setEnablePhase(bool enablePhase) { m_enablePhase = enablePhase; return *this; }
    Plan& setPhase(const std::pair<int, int>& phase) { m_phase = phase; return *this; }
    Plan& serPerfLeader(const std::string leader) { m_leader = leader; return *this; }
    Plan& setPerfPeriod(uint64_t period) { m_period = period; return *this; }
    Plan& addPerfMember(const std::string& member) { m_member.push_back(member); return *this; }
    Plan& addPerfMember(const std::vector<std::string>& member) {
        m_member.insert(m_member.end(), member.begin(), member.end());
        return *this;
    }

    const std::string& getID() const { return m_id; }

    const Task& getTask() const { return m_task; }
    const std::vector<std::string>& getParam() const { return m_param; }
    bool isRT() const { return m_rt; }
    bool needPinCPU() const { return m_pincpu; }

    bool enbalePhase() { return m_enablePhase; }
    std::pair<int, int> getPhase() const { return m_phase; }
    const std::string& gerPerfLeader() const { return m_leader; }
    uint64_t getPerfPeriod() const { return m_period; }
    const std::vector<std::string>& getPerfMember() const { return m_member; }

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
