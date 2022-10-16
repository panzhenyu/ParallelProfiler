#include <poll.h>
#include <iostream>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/sysinfo.h>
#include <sys/signalfd.h>
#include <boost/algorithm/string.hpp>
#include <perfmon/pfmlib_perf_event.h>
#include "PosixUtil.hpp"
#include "ParallelProfiler.hpp"

using Utils::Perf::PerfEventError;
using Utils::Posix::Process;
using Utils::Posix::File;

//----------------------------------------------------------------------------//
// PerfProfiler

/**
 * @brief Callback for Event::ProcessEvents to collect samples.
 * @param[in] e         pointer to an Event object
 * @param[in] v         used for storing samples, whose type is std::vector<sample_t>*
 * @param[in] status    type of record, a non-zero value means the type isn't PERF_RECORD_SAMPLE
 */
static void collectSample(Utils::Perf::Event* e, void* v, int status) {
    using sample_t = PerfProfiler::sample_t;

    bool hasID;
    sample_t sample;
    uint64_t readfmt, nr, i;
    std::vector<sample_t>* samples = static_cast<std::vector<sample_t>*>(v);

    if (status) { return; }
    if (nullptr == e || nullptr == v) { return; }

    readfmt = e->GetReadFormat();
    nr = e->Read<uint64_t>();

    // skip PERF_FORMAT_TOTAL_TIME_ENABLED
    if (readfmt & PERF_FORMAT_TOTAL_TIME_ENABLED) { e->Read<uint64_t>(); }

    // skip PERF_FORMAT_TOTAL_TIME_RUNNING
    if (readfmt & PERF_FORMAT_TOTAL_TIME_RUNNING) { e->Read<uint64_t>(); }

    // collect child events
    hasID = (PERF_FORMAT_ID & readfmt) == 1;
    for (i=0; i<nr; ++i) {
        sample.emplace_back(e->Read<uint64_t>());
        // skip PERF_FORMAT_ID
        if (hasID) { e->Read<uint64_t>(); }
    }

    samples->emplace_back(sample);
}

bool
PerfProfiler::collect(EventPtr event, std::vector<sample_t>& data) {
    // Event is null, collect false.
    if (nullptr == event) { return false; }

    // Not a sampling event, collect failed.
    if (0 == event->GetSamplePeriod()) { return false; }

    /**
     * TODO: Now we haven't support other sample type(except PERF_SAMPLE_READ) yet.
     * If sample type isn't PERF_SAMPLE_READ, we cannot process it.
     */
    if (PERF_SAMPLE_READ != event->GetSampleType()) { return false; }

    // The read format must enable PERF_FORMAT_GROUP.
    if (0 == (PERF_FORMAT_GROUP & event->GetReadFormat())) { return false; }

    // Collect samples.
    event->ProcessEvents(collectSample, &data);

    return true;
}

bool
PerfProfiler::collect(EventPtr event, sample_t& data) {
    // Event is null, collect false.
    if (nullptr == event) { return false; }

    uint64_t readfmt = event->GetReadFormat();
    bool hasID = readfmt & PERF_FORMAT_ID;

    // The read format must enable PERF_FORMAT_GROUP.
    if (0 == (readfmt & PERF_FORMAT_GROUP)) { return false; }

    // Add for nr.
    int skip = 1;

    // Add for time_enabled.
    if (readfmt & PERF_FORMAT_TOTAL_TIME_ENABLED) { skip++; }

    // Add for time_running.
    if (readfmt & PERF_FORMAT_TOTAL_TIME_RUNNING) { skip++; }

    // Size to read.
    size_t size = (skip + (1 + (hasID ? 1 : 0)) * (event->GetChildNum() + 1));
    uint64_t values[size];

    if (-1 == read(event->GetFd(), &values, size * sizeof(uint64_t)) == -1) { return false; }

    // Collect evet count.
    for(size_t i=0, idx; i<=event->GetChildNum(); i++) {
        idx = hasID ? (i<<1)+skip : i+skip;
        data.emplace_back(values[idx]);
    }

    return true;
}

//----------------------------------------------------------------------------//
// ParallelProfiler

/**
 * @brief   Setup function for Utils::Posix::Process::start.
 *          We are in the child process, just configure it.
 * 
 * @param[in] task A Task object contains initialize information for a static task.
 * 
 * @returns 0 if setup succeed, otherwise returns -errno. 
 */
static int
setupSyncTask(const Task& task) {
    sigset_t mask;
    const auto& dir = task.getDir();

    // Chdir to working root for this child.
    if (!dir.empty() && -1 == chdir(dir.c_str())) {
        std::cout << "chdir failed for task[" << task.getID() << "]." << std::endl;
        return -errno;
    }
    
    // Set traceme for synchronize.
    if (-1 == ptrace(PTRACE_TRACEME)) {
        std::cout << "trace failed for task[" << task.getID() << "]." << std::endl;
        return -errno;
    }

    // Enable SIGIO, cause father may block SIGIO before fork this child.
    sigaddset(&mask, SIGIO);
    if (-1 == sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
        std::cout << "enable SIGIO failed for plan[" << task.getID() << "]." << std::endl;
        return -errno;
    }

    return 0;
}

/**
 * Create signalfd to handle these signal.
 * We must ignore SIGCHLD before start process, or SIGCHLD may be handled by default handler,
 * notice that the blocked signals will be derived by children in this way.
 * 
 * SIGINT: Send SIGKILL for all children, terminate profiler without output.
 * SIGIO: Ignore SIGIO, cause signal driven IO send signal to the group.
 * SIGCHLD: Do nothing with this signal, whose default behavior is ignore.
 */
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
ParallelProfiler::killChild(pid_t pid) {
    if (!m_pidmap.count(pid)) { return false; }

    RunningConfig& config = m_pidmap.at(pid);

    switch (config.m_status) {
    case RunningConfig::RUN:
    case RunningConfig::STOP:
        if (-1 != kill(pid, SIGKILL)) {
            config.m_status = RunningConfig::DEAD;
        } else { return false; }
        break;
    case RunningConfig::DEAD:
        break;
    }
    return true;
}

bool
ParallelProfiler::killAll() {
    bool ok = true;
    for (auto& pair : m_pidmap) {
        ok = (ok && killChild(pair.first));
    }
    return ok;
}

bool
ParallelProfiler::wakeupChild(pid_t pid) {
    if (!m_pidmap.count(pid)) { return false; }

    RunningConfig& config = m_pidmap.at(pid);
    if (config.m_status != RunningConfig::STOP || -1 == ptrace(PTRACE_CONT, pid, NULL, NULL)) {
        return false;
    }
    config.m_status = RunningConfig::RUN;

    return true;
}

bool
ParallelProfiler::wakeupAll() {
    for (auto& pair : m_pidmap) {
        if (RunningConfig::STOP == pair.second.m_status) {
            if (!wakeupChild(pair.first)) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Do profile here, use waitpid to sync all child process.
 * Note that a SIGCHLD may caused by many child processes, so use loop to wait for other child in same group.
 * Other children may in running When a child terminated(exit or by signal), so we should kill all children at once.
 * 
 * Handle status for SIGCHLD.
 * WIFEXITED: Child terminated normally if profile status isn't READY, otherwise terminate profiler normally.
 * WIFSIGNALED: Child terminated by signal, error occurs, terminate profiler without output.
 * WIFSTOPPED: Child stopped by signal, check signal with WSTOPSIG(status).
 * 
 * Process signal for child according to each profile status.
 * READY:   Only accept SIGTRAP from children, if not then return false.
 *          Wakeup all children When they are ready, and step into INIT or PROFILE according to phase conditions.
 * INIT:    Step phaseno when accept SIGIO, deliver other signal for child.
 *          A phase condition is satisfied when child phaseno matches its start point.
 *          If phaseno exceed its start point, we should abort.
 *          Step into PROFILE when all children meets their phase condition.
 * PROFILE: Step phaseno and collect data when accept SIGIO, deliver other signal for child.
 * OTHER:   Something goes wrong when do the profile, abort anyway.
 * 
 * @returns false when syscall failed, otherwise return true.
 */
bool
ParallelProfiler::handleChild(pid_t pid) {
    pid_t gid;
    int fd, status, signo;
    ProfileStatus profStatus = getStatus();

    if (profStatus >= DONE) {
        std::cout << "error profile status[" << profStatus << "] while processing signal for child." << std::endl;
        return false;
    }

    while (-1 != (pid=waitpid(0, &status, WNOHANG))) {
        if (0 == pid) { break; }

        // This if clause guarantees that every processed pid is in m_pidmap.
        // So there is no need to check whether pid is valid in following processes, such as pid in m_pstatus.
        if (!m_pidmap.count(pid)) {
            std::cout << "unknown pid[" << pid << "] while processing signal for child." << std::endl;
            setStatus(ProfileStatus::ABORT);
            break;
        }

        RunningConfig& config = m_pidmap.at(pid);
        const Plan& plan = config.m_plan;

        std::cout << "get plan: " << plan.getID() << " signal: " << WSTOPSIG(status) << 
            " stop by signal?: " << WIFSTOPPED(status) << 
            " terminated by signal?: " << WIFSIGNALED(status) << 
            " exit normally?: " << WIFEXITED(status) << std::endl;

        if (WIFEXITED(status)) {
            // When we in here, there is a child exit normally.

            // Set DEAD status.
            config.m_status = RunningConfig::DEAD;

            if (ProfileStatus::READY == profStatus) {
                // Child exit at READY stage means a process may start failed, abort.
                setStatus(ProfileStatus::ABORT);
                std::cout << "failed to start plan[" << plan.getID() << "]." << std::endl;
            } else if (ProfileStatus::INIT == profStatus) {
                // Child exit at INIT stage means the phase condition may be too large, abort.
                setStatus(ProfileStatus::ABORT);
                std::cout << "plan[" <<  plan.getID() << "] exit at INIT stage." << std::endl;
            } else {
                // Child exit at PROFILE stage.
                // Try to step into DONE stage.
                if (!gotoDONE()) {
                    setStatus(ProfileStatus::ABORT);
                    std::cout << "goto DONE stage failed at plan[" << plan.getID() << "] exit." << std::endl;
                    return true;
                }

                // Succeed to step into DONE stage.
                std::cout << "plan[" <<  plan.getID() << "] exit normally." << std::endl;
            }

            // Break this clause.
            // Otherwise other children will trigger WIFSIGNALED or WIFSTOPPED to change profile status.
            return true;
        } else if (WIFSIGNALED(status)) {
            // Child terminated by signal, abort.
            // Set child status first.
            config.m_status = RunningConfig::DEAD;

            // Set profile status to ABORT.
            setStatus(ProfileStatus::ABORT);

            std::cout << "plan[" << plan.getID() << "] abort by signal[" << WIFSIGNALED(status) << "]." << std::endl;
            
            // There is no need to process other children anymore(profile status may also be modified by other process).
            // So return immediately.
            return true;
        } else if (WIFSTOPPED(status)) {
            // Traced child receive a signal and stopped.
            // Collect the signal number.
            signo = WSTOPSIG(status);

            // Process signal according to profile status.
            switch (profStatus) {
            case READY:
                // Each traced child should start with SIGTRAP(triggered by exec).
                if (SIGTRAP == signo) {
                    // Update status for child, the child should stop to wait for other children.
                    config.m_status = RunningConfig::STOP;

                    // Child pid has reached READY stage, add it into m_pstatus.
                    m_pstatus[ProfileStatus::READY].insert(pid);

                    // The child should skip INIT stage either it doesn't have a phase condition,
                    // or its phase condition has been satisfied.
                    if (plan.getType() != Plan::Type::SAMPLE_PHASE || 0 == plan.getPhase().first) {
                        m_pstatus[ProfileStatus::INIT].insert(pid);
                    }
                } else {
                    // Child shouldn't receive other signal at READY stage.
                    // So we have to abort.
                    setStatus(ProfileStatus::ABORT);
                    return true;
                }

                // Check whether all children have sent SIGTRAP.
                if (m_pidmap.size() == m_pstatus[ProfileStatus::READY].size()) {
                    // READY done, step into INIT/PROFILE stage.

                    if (m_pidmap.size() == m_pstatus[ProfileStatus::INIT].size()) {
                        // If all children pass INIT stage.
                        // We should prepare and step into PROFILE.
                        if (!gotoPROFILE()) {
                            // If failed then return false to report system error.
                            std::cout << "goto PROFILE failed at plan[" << plan.getID() << "] in READY stage." << std::endl;
                            setStatus(ProfileStatus::ABORT);
                            return true;
                        }
                    } else {
                        // Some children need INIT stage to satisfy their phase condition.
                        if (!gotoINIT()) {
                            // If step into INIT stage failed then return false to report system error.
                            std::cout << "goto INIT failed at plan[" << plan.getID() << "] in READY stage." << std::endl;
                            setStatus(ProfileStatus::ABORT);
                            return true;
                        }
                    }

                    // Terminate this handling to handle next stage.
                    return true;
                }

                // Process next child.
                break;

            case INIT:
                // In INIT stage, SIGIO only send to the child who generates an overflow.
                // Only SAMPLE_PHASE plan can step into this stage.
                if (plan.getType() != Plan::Type::SAMPLE_PHASE) {
                    std::cout << "plan[" << plan.getID() << "] shouldn't step into INIT stage." << std::endl;
                    setStatus(ProfileStatus::ABORT);
                    return true;
                }

                // Process signal for this child.
                if (SIGIO == signo) {
                    // Receive SIGIO when an overflow occurs.

                    // Update phaseno for child.
                    if (!updatePhase(pid)) {
                        setStatus(ProfileStatus::ABORT);
                        std::cout << "update phase failed for plan[" << plan.getID() << "] at INIT stage." << std::endl;
                        return true;
                    }

                    // Check whether phase condition is satisfied.
                    if (config.m_phaseno >= plan.getPhase().first) {
                        // Child finishs INIT stage, add it into m_pstate.
                        m_pstatus[ProfileStatus::INIT].insert(pid);

                        // Wait for other child.
                        config.m_status = RunningConfig::STOP;

                        // Stop the perf event.
                        try {
                            config.m_event->Stop();
                        } catch (PerfEventError e) {
                            std::cout << "failed to stop perf event for plan[" << plan.getID() << "]." << std::endl;
                            return false;
                        }
                    } else if (-1 == ptrace(PTRACE_CONT, pid, NULL, NULL)) {
                        // Try to wake up this child for next phase.
                        // Return false to report system error if wake up failed.
                        return false;
                    }
                } else {
                    // Child receive other signal, just deliver it.
                    if (-1 == ptrace(PTRACE_CONT, pid, NULL, (void*)((long)signo))) {
                        std::cout << "failed to deliver signal[" << signo << 
                            "] to plan[" << plan.getID() << "]." << std::endl;
                        return false;
                    }
                }

                if (m_pidmap.size() == m_pstatus[ProfileStatus::INIT].size()) {
                    // INIT done, step into PROFILE stage.
                    if (!gotoPROFILE()) {
                        // If failed then return false to report system error.
                        std::cout << "goto PROFILE failed at plan[" << plan.getID() << "] in INIT stage." << std::endl;
                        setStatus(ProfileStatus::ABORT);
                        return true;
                    }

                    // Terminate this handling to handle next stage.
                    return true;
                }

                // Process next child.
                break;

            case PROFILE:
                /**
                 * In PROFILE stage, SIGIO will be send for group, which means all children
                 * will recv SIGIO and deliver it to main process.
                 */
                if (SIGIO == signo) {
                    // Use m_pstatus[PROFILE] to sync all children here.
                    m_pstatus[ProfileStatus::PROFILE].insert(pid);
                    config.m_status = RunningConfig::STOP;
                } else {
                    if (-1 == ptrace(PTRACE_CONT, pid, NULL, (void*)((long)signo))) {
                        std::cout << "failed to deliver signal[" << signo << 
                            "] to plan[" << plan.getID() << "]." << std::endl;
                        return false;
                    }
                }

                if (m_pidmap.size() == m_pstatus[ProfileStatus::PROFILE].size()) {
                    // Sync done, all children have been stopped by SIGIO, reset m_pstatus to prepare for next sync.
                    m_pstatus[ProfileStatus::PROFILE].clear();

                    // Collect data for perf plan and update phase for sample plan.
                    collectAll();

                    // Check phase condition.
                    for (auto& [pid, config] : m_pidmap) {
                        const Plan& plan = config.m_plan;
                        if (Plan::Type::SAMPLE_PHASE == plan.getType() && config.m_phaseno >= plan.getPhase().second) {
                            m_pstatus[ProfileStatus::DONE].insert(pid);
                        }
                    }

                    if (!m_pstatus[ProfileStatus::DONE].empty()) {
                        if (!gotoDONE()) {
                            setStatus(ProfileStatus::ABORT);
                            std::cout << "goto DONE stage failed at plan[" << plan.getID() << "] exit." << std::endl;
                        }
                        return true;
                    } else if (!wakeupAll()) {
                        // wake up all children if no child meets phase ending
                        std::cout << "wake up children failed at PROFILE stage." << std::endl;
                        return false;
                    }
                }

                // Process next child.
                break;
            }
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
        // some children may stopped by SIGIO, handle it anyway.
    case SIGCHLD:
        return handleChild(fdsi.ssi_pid);
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
    struct pollfd pfd[1];
    std::vector<int> oldcpuset;
    int status, sfd = 0, err = 0;

    // Reset running config.
    m_pstatus.fill(procset_t());
    setStatus(ProfileStatus::READY);

    // Save cpuset, cause buildRunningConfig will modify m_cpuset.
    oldcpuset = m_cpuset;

    // Check & Build task.
    if (!authCheck() || !argsCheck()) {
        err = -1;
        goto finalize;
    }

    if (PFM_SUCCESS != pfm_initialize()) {
        std::cout << "init libpfm failed" << std::endl;
        err = -2;
        goto finalize;
    }

    if (-1 == (sfd=createSignalFD())) {
        std::cout << "create signal fd failed." << std::endl;
        err = -3;
        goto terminate;
    }

    // Build running config, start all plan.
    for (auto& plan : m_plan) {
        if (!buildRunningConfig(plan)) {
            err = -4;
            goto terminate;
        }
    }

    /**
     * Ready to profile, wait for signal.
     */
    std::cout << "start profiling..." << std::endl;
    while (getStatus() < ProfileStatus::DONE) {
        pfd[0] = { sfd, POLLIN, 0 };
        if (-1 != poll(pfd, sizeof(pfd) / sizeof(*pfd), -1)) {
            if (pfd[0].revents & POLLIN){
                if (!handleSignal(sfd)) {
                    setStatus(ProfileStatus::ABORT);
                    err = -errno;
                }
            }
        } else if (errno != EINTR) {
            std::cout << "poll failed with errno: " << errno << std::endl;
            setStatus(ProfileStatus::ABORT);
            err = -errno;
        } else {
            std::cout << "errno is EINTR" << std::endl;
        }
    }

    /**
     * Profile done or abort as we get here.
     * Output perf record if the profile has done.
     */
    if (ProfileStatus::DONE == getStatus()) {
        /**
         * TODO: output result
         */
    }

    err = getStatus();

terminate:
    killAll();
    while (-1 != (ret=waitpid(0, NULL, 0))) {
        std::cout << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
            " stop by signal?: " << WIFSTOPPED(status) << 
            " terminated by signal?: " << WIFSIGNALED(status) << 
            " exit normally?: " << WIFEXITED(status) << std::endl;
    }

finalize:
    m_pidmap.clear();
    if (sfd > 0) { close(sfd); }
    pfm_terminate();
    m_cpuset = oldcpuset;

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

bool
ParallelProfiler::updatePhase(pid_t pid) {
    std::vector<sample_t> samples;

    if (!m_pidmap.count(pid)) { return false; }

    RunningConfig config = m_pidmap.at(pid);
    EventPtr event = config.m_event;
    const Plan& plan = config.m_plan;

    // Non-sample plan can not update phase.
    if (!plan.samplePlan()) { return false; }

    // Collect samples for this config and step phaseno.
    if (!collect(event, samples)) {
        std::cout << "collect failed for plan[" << plan.getID() << "]." << std::endl;
        return false;
    }

    // Update phaseno.
    config.m_phaseno += samples.size();

    return true;
}

bool
ParallelProfiler::collectAll() {
    sample_t sample;

    // We can only collect sample on PROFILE stage.
    if (ProfileStatus::PROFILE != getStatus()) { return false; }

    // Collect data and update phase.
    for (auto& [pid, config] : m_pidmap) {
        EventPtr event = config.m_event;
        const Plan& plan = config.m_plan;
        auto& samples = config.m_samples;

        // If plan isn't a perf plan, just skip it.
        if (!plan.perfPlan()) { continue; }

        // Collect sample.
        if (!collect(event, sample)) {
            // Collect failed, return false;
            std::cout << "collect failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }

        // Save sample.
        samples.emplace_back(sample);

        // Update phase for sample plan.
        if (plan.samplePlan() && !updatePhase(pid)) { return false; }

        // Reset perf counter.
        try {
            event->Reset();
        } catch (PerfEventError e) {
            std::cout << "failed to reset perf event for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }
    }

    // Collect & update done.
    return true;
}

bool
ParallelProfiler::gotoINIT() {
    // Some children need INIT stage to satisfy their phase condition.
    // Step into INIT stage.

    int fd;

    // We can only step into INIT from READY stage.
    if (ProfileStatus::READY != getStatus()) { return false; }

    // Setup for children need INIT stage.
    for (pid_t readyChild : m_pstatus[ProfileStatus::READY]) {
        // Ignore those who skip the INIT stage.
        if (m_pstatus[ProfileStatus::INIT].count(readyChild)) { continue; }

        auto& config = m_pidmap.at(readyChild);
        EventPtr event = config.m_event;
        const Plan& plan = config.m_plan;

        // Get file descriptor of perf event.
        fd = event->GetFd();

        // Set signal driven IO for sample plan.
        if (!File::enableSigalDrivenIO(fd)) {
            // Failed to enable signal driven io, return false to report system error.
            std::cout << "enable signal driven io failed for plan[" <<
                plan.getID() << "]." << std::endl;
            return false;
        }
        
        // Set file owner to child itself, cause INIT stage doesn't need to synchronize all children.
        if (!File::setFileOwner(fd, readyChild)) {
            // Failed to set file owner, return false to report system error.
            std::cout << "set file owner failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }

        // Reset and Start the perf event.
        try {
            event->Reset();
            event->Start();
        } catch (PerfEventError e) {
            std::cout << "failed to reset and start perf event for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }

        // Wake up this child.
        if (!wakeupChild(readyChild)) { return false; }
    }

    // All preparation done, step into INIT.
    setStatus(ProfileStatus::INIT);

    return true;
}

bool
ParallelProfiler::gotoPROFILE() {
    int fd;
    pid_t gid;

    // Only READY and INIT can step into PROFILE.
    if (ProfileStatus::READY != getStatus() && ProfileStatus::INIT != getStatus()) { return false; }

    // Get process group id.
    gid = getpgrp();

    // Prepare perf event for every sample plan.
    for (auto& [pid, config] : m_pidmap) {
        EventPtr event = config.m_event;
        const Plan& plan = config.m_plan;

        // Skip non-sample plan.
        if (!plan.samplePlan()) { continue; }

        // Get perf event fd.
        fd = event->GetFd();

        // Set signal driven IO.
        if (!File::enableSigalDrivenIO(fd)) {
            // Failed to enable signal driven io, return false to report system error.
            std::cout << "enable signal driven io failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }
        
        // Set file owner to process group, so that all children can be synchronized by one SIGIO.
        if (!File::setFileOwner(fd, -gid)) {
            // Failed to set file owner, return false to report system error.
            std::cout << "set file owner failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }

        // Reset and Start the perf event.
        try {
            event->Reset();
            event->Start();
        } catch (PerfEventError e) {
            std::cout << "failed to reset and start perf event for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }
    }

    // Wake up all children.
    if (!wakeupAll()) {
        std::cout << "wake up children failed at READY stage." << std::endl;
        return false;
    }

    // All preparation done, step into PROFILE.
    setStatus(ProfileStatus::PROFILE);

    return true;
}

bool
ParallelProfiler::gotoDONE() {
    sample_t sample;

    // Only PROFILE stage can step into DONE.
    if (ProfileStatus::PROFILE != getStatus()) { return false; }

    // Collect rest data and update phase for sample plan.
    if (!collectAll()) { return false; }

    // Stop perf event for perf plan.
    for (auto& [pid, config] : m_pidmap) {
        EventPtr event = config.m_event;
        const Plan& plan = config.m_plan;

        if (plan.perfPlan()) {
            try {
                event->Stop();
            } catch (PerfEventError e) {
                std::cout << "failed to reset and start perf event for plan[" << plan.getID() << "]." << std::endl;
                return false;
            }
        }
    }
    
    // Collect & stop done, step into DONE.
    setStatus(ProfileStatus::DONE);

    return true;
}

// build task for plan
bool
ParallelProfiler::buildRunningConfig(const Plan& plan) {
    int cpu = -1;
    pid_t pid = -1;
    EventPtr event = nullptr;
    RunningConfig conf(plan);

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

    // Start process.
    if (-1 == (pid=Process::start(std::bind(setupSyncTask, plan.getTask()), cmdVector))) { return false; }

    // pin cpu
    if (plan.needPinCPU()) {
        if (m_cpuset.empty()) {
            std::cout << "cpuset isn't enough for plan[" << plan.getID() << "]." << std::endl;
            goto killchild;
        }
        cpu = m_cpuset.back();
        if (!Process::setCPUAffinity(pid, cpu)) {
            std::cout << "pin cpu failed for plan[" << plan.getID() << "] with errno[" << errno << "]." << std::endl;
            goto killchild;
        }
        m_cpuset.pop_back();
    }

    // set rt process
    if (plan.isRT() && !Process::setFIFOProc(pid, sched_get_priority_max(SCHED_FIFO))) {
        std::cout << "set fifo failed for plan[" << plan.getID() << "] with errno[" << errno << "]." << std::endl;
        goto killchild;
    }

    // register perf event
    if (plan.perfPlan()) {
        /**
         * TODO: init perf event here
         */
        if (nullptr == event) {
            std::cout << "register perf event failed  for plan[" << plan.getID() << "]." << std::endl;
            goto killchild;
        }
    }

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
