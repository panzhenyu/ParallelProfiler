#include <iostream>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/sysinfo.h>
#include <perfmon/pfmlib_perf_event.h>
#include <boost/algorithm/string.hpp>
#include "ParallelProfiler.hpp"

int
ParallelProfiler::profile() {
    bool ok;
    pid_t ret;
    int status, err = 0;

    // check & build task
    if (!authCheck() || !argsCheck()) {
        err = -1;
        goto free;
    }

    if (PFM_SUCCESS != pfm_initialize()) {
        std::cout << "init libpfm failed" << std::endl;
        err = -2;
        goto terminate;
    }

    for (auto& plan : m_plan) {
        if (!addRunningConfig(plan)) {
            err = -3;
            goto terminate;
        }
    }

    /**
     * When we get here, all process are started normally, we shouldn't use goto anymore.
     * Use killAll to kill child, then waitpid will handle signal for child normally.
     */

    /**
     * Handle signal for main process.
     * SIGINT: Send SIGKILL for all children, terminate profiler without output.
     * SIGIO: Ignore SIGIO, cause this profiler bind SIGIO to children.
     * SIGCHLD: Do nothing with this signal, whose default behavior is ignore.
     */

    // Set signal driven IO here for sample plan.

    // do profile here, use wait to sync all child process
    while (-1 != (ret=waitpid(0, &status, 0))) {

        /**
         * Handle status for SIGCHLD.
         * WIFEXITED: Child terminated normally, terminate profiler normally.
         * WIFSIGNALED: Child terminated by signal, error occurs, terminate profiler without output.
         * WIFSTOPPED: Child stopped by signal, check signal with WSTOPSIG(status).
         */
        /**
         * Handle concrete signal WSTOPSIG(status).
         * SIGTRAP: Execve done, child process start successfully, sync phase for all children.
         * SIGIO: A sample overflow occurred, wait for other children, collect data and send PTRACE_CONT to continue.
         * OTHER: Deliver this signal to the stopped child.
         */
        std::cout << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
            " stop by signal?: " << WIFSTOPPED(status) << 
            " terminated by signal?: " << WIFSIGNALED(status) << 
            " exit normally?: " << WIFEXITED(status) << std::endl;
        ptrace(PTRACE_CONT, ret, NULL, NULL);
    }
    // All children have been terminated when we get here.

    // output perf event record

terminate:
    pfm_terminate();

free:
    m_pidmap.clear();
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

static int
setupSyncTask(const Plan& plan) {
    // here we are in child process, configure it.
    // set traceme
    if (-1 == ptrace(PTRACE_TRACEME)) {
        std::cout << "trace failed for plan[" << plan.getID() << "]." << std::endl;
        return -errno;
    }
    return 0;
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

bool
ParallelProfiler::collect() {
    return true;
}


void
ParallelProfiler::killAll() {
    for (auto& pair : m_pidmap) {
        kill(pair.first, SIGKILL);
    }
}
