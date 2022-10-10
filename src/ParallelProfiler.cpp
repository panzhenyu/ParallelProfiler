#include <poll.h>
#include <iostream>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/sysinfo.h>
#include <sys/signalfd.h>
#include <boost/algorithm/string.hpp>
#include <perfmon/pfmlib_perf_event.h>
#include "ParallelProfiler.hpp"

/**
 * We are in child process, configure it.
 */
static int
setupSyncTask(const Plan& plan) {
    const auto& dir = plan.getTask().getDir();

    // chdir
    if (!dir.empty() && -1 == chdir(dir.c_str())) {
        std::cout << "chdir failed for plan[" << plan.getID() << "]." << std::endl;
        return -errno;
    }
    
    // set traceme
    if (-1 == ptrace(PTRACE_TRACEME)) {
        std::cout << "trace failed for plan[" << plan.getID() << "]." << std::endl;
        return -errno;
    }
    return 0;
}

static int
createSignalFD() {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGIO);

    if (-1 != sigprocmask(SIG_BLOCK, &mask, NULL)) {
        return signalfd(-1, &mask, 0);
    }

    return -1;
}

bool
ParallelProfiler::collect(record_t& record) {
    record.clear();
    return true;
}

void
ParallelProfiler::killAll() {
    for (auto& pair : m_pidmap) {
        switch (pair.second.m_status) {
            case RunningConfig::RUN:
            case RunningConfig::STOP:
                if (-1 != kill(pair.first, SIGKILL)) {
                    pair.second.m_status = RunningConfig::DEAD;
                }
                break;
            case RunningConfig::DEAD:
                break;
        }
    }
}

/**
 * Do profile here, use waitpid to sync all child process.
 * 
 * Handle status for SIGCHLD.
 * WIFEXITED: Child terminated normally, terminate profiler normally.
 * WIFSIGNALED: Child terminated by signal, error occurs, terminate profiler without output.
 * WIFSTOPPED: Child stopped by signal, check signal with WSTOPSIG(status).
 * 
 * Handle concrete signal WSTOPSIG(status).
 * SIGTRAP: Execve done, child process start successfully, sync phase for all children.
 * SIGIO: A sample overflow occurred, wait for other children, collect data and send PTRACE_CONT to continue.
 * OTHER: Deliver this signal to the stopped child.
 * 
 * All children are exited when waitpid returns -1(see man waitpid).
 */
bool
ParallelProfiler::handleChild(pid_t pid) {
    pid_t ret;
    int status;

    while (-1 != (ret=waitpid(0, &status, WNOHANG))) {
        if (0 == ret) { break; }

        std::cout << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
            " stop by signal?: " << WIFSTOPPED(status) << 
            " terminated by signal?: " << WIFSIGNALED(status) << 
            " exit normally?: " << WIFEXITED(status) << std::endl;

        if (WIFEXITED(status)) {
            setStatus(ProfileStatus::DONE);
        } else if (WIFSIGNALED(status)) {
            setStatus(ProfileStatus::ABORT);
        } else {
            // process signal here
            ptrace(PTRACE_CONT, ret, NULL, NULL);
        }
    }
    return true;
}

bool
ParallelProfiler::handleSignal(int sfd) {
    struct signalfd_siginfo fdsi;

    if (sizeof(fdsi) != read(sfd, &fdsi, sizeof(fdsi))) {
        std::cout << "read siginfo failed." << std::endl;
        return false;
    }

    switch (fdsi.ssi_signo) {
        case SIGINT:
            std::cout << "receive SIGINT, stop profiling" << std::endl;
            setStatus(ProfileStatus::ABORT);
            break;
        case SIGIO:
            // main process should ignore this signal
            break;
        case SIGCHLD:
            if (!handleChild(fdsi.ssi_pid)) {
                std::cout << "handle child event failed for pid: " << fdsi.ssi_pid << std::endl;
                return false;
            }
            break;
        default:
            std::cout << "Unhandled signal: " << fdsi.ssi_signo << std::endl;
            break;
    }
    return true;
}

int
ParallelProfiler::profile() {
    bool ok;
    pid_t ret;
    int status, sfd, err = 0;
    struct pollfd pfd[1];

    // clean running config
    killAll();
    m_pidmap.clear();

    // check & build task
    if (!authCheck() || !argsCheck()) {
        err = -1;
        goto finalize;
    }

    if (PFM_SUCCESS != pfm_initialize()) {
        std::cout << "init libpfm failed" << std::endl;
        err = -2;
        goto finalize;
    }

    for (auto& plan : m_plan) {
        if (!addRunningConfig(plan)) {
            err = -3;
            goto terminate;
        }
    }

    // Set signal driven IO here for sample plan.


    /**
     * Create signalfd to handle these signal.
     * SIGINT: Send SIGKILL for all children, terminate profiler without output.
     * SIGIO: Ignore SIGIO, cause signal driven IO send signal to the group.
     * SIGCHLD: Do nothing with this signal, whose default behavior is ignore.
     */
    if (-1 == (sfd=createSignalFD())) {
        std::cout << "create signal fd failed." << std::endl;
        err = -5;
        goto terminate;
    }

    /**
     * Ready to profile, wait for signal.
     */
    setStatus(ProfileStatus::READY);
    while (getStatus() < ProfileStatus::DONE) {
        pfd[0] = { sfd, POLLIN, 0 };

        if (poll(pfd, sizeof(pfd) / sizeof(*pfd), -1) != -1) {
            if (pfd[0].revents & POLLIN){
                if (!handleSignal(sfd)) {
                    goto terminate;
                }
            }
        } else if (errno != EINTR) {
            std::cout << "poll failed with errno: " << errno << std::endl;
            goto terminate;
        }
    }

    /**
     * All children terminated as we get here.
     * Output perf record if the profile has done.
     */
    if (ProfileStatus::DONE == getStatus()) {

    }

terminate:
    killAll();
    while (-1 != (ret=waitpid(0, &status, 0))) {
        std::cout << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
            " stop by signal?: " << WIFSTOPPED(status) << 
            " terminated by signal?: " << WIFSIGNALED(status) << 
            " exit normally?: " << WIFEXITED(status) << std::endl;
    }
    pfm_terminate();

finalize:
    std::vector<int>().swap(m_cpuset);
    std::vector<Plan>().swap(m_plan);

    return err;
}

bool
ParallelProfiler::authCheck() {
    if (0 != geteuid()) {
        std::cout << "error: unprivileged user[" << getuid() << "]." << std::endl;
        return false;
    }
    return true;
}

bool
ParallelProfiler::argsCheck() {
    std::vector<int> validCPU;
    int nrNeededCPU, nrCPU = get_nprocs();

    // collect valid cpu
    for (auto cpuno : m_cpuset) {
        if (cpuno >= 0 && cpuno < nrCPU) {
            validCPU.emplace_back(cpuno);
        }
    }
    // count needed cpu
    nrNeededCPU = std::count_if(m_plan.begin(), m_plan.end(), 
        [] (const Plan& plan) -> bool { return plan.needPinCPU(); }
    );

    // check cpu
    if (validCPU.size() < nrNeededCPU) {
        std::cout << "nrValidCPU" << validCPU.size() << "] is smaller than nrNeededCPU[" << nrNeededCPU << "]." << std::endl;
        return false;
    }

    // check plan
    for (const auto& plan : m_plan) {
        if (!plan.valid()) {
            std::cout << "plan[" << plan.getID() << "] is invalid." << std::endl;
            return false;
        }
    }

    // swap current cpu set into valid cpu set
    m_cpuset.swap(validCPU);

    return true;
}

// build task for plan
bool
ParallelProfiler::addRunningConfig(const Plan& plan) {
    int cpu = -1;
    pid_t pid = -1;
    EventPtr event = nullptr;
    RunningConfig conf(plan);

    cpu_set_t cpuset;
    struct sched_param param;

    std::string cmd, var;
    std::vector<std::string> args, cmdVector;

    // gen cmd args and create process
    cmd = plan.getTask().getCmd();
    args = plan.getParam();
    if (!cmd.empty()) {
        for (int j=0; j<args.size(); ++j) {
            var = std::string(Task::ARG_PREFIX) + std::to_string(j+Task::ARG_INDEX_BEGIN);
            boost::replace_all(cmd, var, args[j]);
        }
        if (!cmd.empty()) {
            boost::split(cmdVector, cmd, boost::is_any_of(" "), boost::token_compress_on);
        }
    }
    if (-1 == (pid=Process::start(std::bind(setupSyncTask, plan), cmdVector))) {
        return false;
    }

    // pin cpu
    if (plan.needPinCPU()) {
        if (m_cpuset.empty()) {
            std::cout << "cpuset isn't enough for plan[" << plan.getID() << "]." << std::endl;
            goto killchild;
        }
        cpu = m_cpuset.back();
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        if (1 == sched_setaffinity(0, sizeof(cpu_set_t), &cpuset)) {
            std::cout << "pin cpu failed for plan[" << plan.getID() << "] with errno[" << errno << "]." << std::endl;
            goto killchild;
        }
        m_cpuset.pop_back();
    }

    // set rt process
    if (plan.isRT()) {
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (1 == sched_setscheduler(0, SCHED_FIFO, &param)) {
            std::cout << "set fifo failed for plan[" << plan.getID() << "] with errno[" << errno << "]." << std::endl;
            goto killchild;
        }
    }

    // register perf event

    // add running config
    conf.m_pid = pid;
    conf.m_cpu = cpu;
    conf.m_event = event;
    if (!m_pidmap.emplace(pid, conf).second) {
        std::cout << "emplace running config failed for plan[" << plan.getID() << "]." << std::endl;
        goto killchild;
    }

    return true;

killchild:
    kill(pid, SIGKILL);
    return false;
}
