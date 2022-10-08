#include <iostream>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/sysinfo.h>
#include <boost/algorithm/string.hpp>
#include "ParallelProfiler.hpp"

static int setupSyncTask(const Plan& plan, int cpu) {
    // here we are in child process, configure it.
    cpu_set_t cpu_set;
    struct sched_param param;

    // set traceme
    if (-1 == ptrace(PTRACE_TRACEME)) {
        std::cout << "trace failed for plan[" << plan.getID() << "]." << std::endl;
        return -errno;
    }

    // pin cpu
    if (plan.needPinCPU()) {
        CPU_ZERO(&cpu_set);
        CPU_SET(cpu, &cpu_set);
        if (-1 == sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set)) {
            std::cout << "pin cpu failed for plan[" << plan.getID() << "]." << std::endl;
            return -errno;
        }
    }

    // set rt process
    if (plan.isRT()) {
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (-1 == sched_setscheduler(0, SCHED_FIFO, &param)) {
            std::cout << "set fifo failed for plan[" << plan.getID() << "]." << std::endl;
            return -errno;
        }
    }

    return 0;
}

int ParallelProfiler::profile() {
    gid_t gid;
    pid_t ret;
    int status;

    if (!authCheck() || !argsCheck() || !buildTask()) {
        return -1;
    }

    // do profile here, use wait to sync all child process
    gid = getgid();
    while (-1 != (ret=waitpid(-gid, &status, 0))) {
        // child start success with SIGHLD
        ptrace(PTRACE_CONT, ret, NULL, NULL);
    }

    return 0;
}

bool ParallelProfiler::authCheck() {
    if (0 != geteuid()) {
        std::cout << "error: unprivileged user[" << getuid() << "]." << std::endl;
        return false;
    }
    return true;
}

bool ParallelProfiler::argsCheck() {
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

// build task for each plan
bool ParallelProfiler::buildTask() {
    int cpu;
    Process proc;
    std::string cmd, var;
    std::vector<std::string> args, cmdVector;

    for (int i=0, cpuIdx=0; i<m_plan.size(); ++i) {
        // get reference of current plan
        const auto& plan = m_plan[i];

        // prepare cmd arguments for process
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

        // build setup function & process
        cpu = plan.needPinCPU() ? cpuIdx++ : -1;
        auto setup = std::bind(setupSyncTask, plan, cpu);
        if (proc.start(setup, cmdVector)) {
            proc.setCPU(cpu);
            proc.setStatus(ProcessStatus::READY);
            m_process.emplace_back(proc);
            m_pidmap[proc.getPid()] = i;
        } else {
            std::cout << "start plan[" << plan.getID() << "] failed." << std::endl;
            return false;
        }
    }
    return true;
}
