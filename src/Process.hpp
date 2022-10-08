#include <string>
#include <vector>
#include <errno.h>
#include <unistd.h>
#include <functional>

class Process {
public:
    Process(): m_pid(-1), m_cpu(-1) {}
    Process(const Process&) = default;
    ~Process() = default;
    
    bool start(const std::function<int()>& func) {
        m_pid = fork();
        if (0 == m_pid) {
            exit(func());
        } else if (-1 == m_pid) {
            return false;
        }
        return true;
    }

    bool start(const std::function<int()>& setup, const std::vector<std::string>& args) {
        int err;
        std::vector<char*> argv;

        if (0 == args.size()) {
            return false;
        }

        m_pid = fork();

        if (0 == m_pid) {
            // prepare args
            for (auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(0);
            // setup for this child process
            if (0 != (err=setup())) {
               exit(err);
            }
            // do exec
            if (-1 == execve(args[0].c_str(), argv.data(), {0})) {
               exit(-errno);
            }
        } else if (-1 == m_pid) {
            return false;
        }

        return true;
    }

    void setCPU(int cpu) { m_cpu = cpu; }
    void setStatus(int status) { m_status = status; }
    pid_t getPid() const { return m_pid; }
    int getCPU() const { return m_cpu; }
    int getStatus() const { return m_status; }

protected:
    pid_t   m_pid;
    int     m_cpu;
    int     m_status;
};
