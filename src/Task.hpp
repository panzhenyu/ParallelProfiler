#include <string>
#include <vector>

struct IProcess {
    enum ProcessStatus { INIT, START, STOP, KILLED };

    IProcess() = default;
    IProcess(const IProcess&) = default;
    virtual ~IProcess() = default;

    virtual int start(const std::string&, const std::vector<std::string>&) = 0;
    virtual int setRT() = 0;
    virtual int setCPU(int) = 0;
    virtual void setStatus(ProcessStatus) = 0;

    virtual pid_t getPid() = 0;
    virtual int getCPU() = 0;
    virtual ProcessStatus getStatus() = 0;
};

class TracedProcess: IProcess {
public:
    TracedProcess(): m_pid(-1), m_cpu(-1) {}
    TracedProcess(const TracedProcess&) = default;
    ~TracedProcess() = default;
    
    virtual int start(const std::string&, const std::vector<std::string>&);
    virtual int setRT();
    virtual int setCPU(int);
    virtual void setStatus(ProcessStatus status) { m_status = status; }

    virtual pid_t getPid() { return m_pid; }
    virtual int getCPU() { return m_cpu; }
    virtual ProcessStatus getStatus() { return m_status; }

protected:
    pid_t           m_pid;
    int             m_cpu;
    bool            m_rt;
    ProcessStatus   m_status;
};
