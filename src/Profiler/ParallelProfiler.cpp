#include <poll.h>
#include <iostream>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/sysinfo.h>
#include <sys/signalfd.h>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <perfmon/pfmlib_perf_event.h>
#include "PosixUtil.hpp"
#include "ParallelProfiler.hpp"

using Utils::Perf::PerfEventError;
using Utils::Posix::Process;
using Utils::Posix::File;

//----------------------------------------------------------------------------//
// TODO: Tools, delete later

std::ostream& operator<<(std::ostream& out, const ParallelProfiler::sample_t& sample) {
    for (auto x : sample) {
        out << x << " ";
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const std::vector<ParallelProfiler::sample_t>& samples) {
    for (auto& sample : samples) {
        out << sample << std::endl;
    }
    return out;
}

//----------------------------------------------------------------------------//
// ParallelProfiler

ParallelProfiler::RunningConfig::RunningConfig(const Plan& plan)
    : m_pid(-1), m_cpu(-1), m_plan(plan), m_event(nullptr), m_phaseno(0), m_status(Status::RUN) {}

int
ParallelProfiler::setupSyncTask(const Task& task) {
    sigset_t mask;
    const auto& dir = task.getDir();

    // Chdir to working root for this child.
    if (!dir.empty() && -1 == chdir(dir.c_str())) {
        // m_log << "chdir failed for task[" << task.getID() << "]." << std::endl;
        return -errno;
    }
    
    // Set traceme for synchronize.
    if (-1 == ptrace(PTRACE_TRACEME)) {
        // m_log << "trace failed for task[" << task.getID() << "]." << std::endl;
        return -errno;
    }

    // Enable OVERFLOW_SIG, cause father may block signal before fork this child.
    sigaddset(&mask, ParallelProfiler::OVERFLOW_SIG);
    if (-1 == sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
        // m_log << "enable OVERFLOW_SIG failed for plan[" << task.getID() << "]." << std::endl;
        return -errno;
    }

    return 0;
}

int
ParallelProfiler::createSignalFD() {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, ParallelProfiler::OVERFLOW_SIG);

    if (-1 != sigprocmask(SIG_BLOCK, &mask, NULL)) {
        // We do not handle OVERFLOW_SIG, just ignore it.
        sigdelset(&mask, ParallelProfiler::OVERFLOW_SIG);
        return signalfd(-1, &mask, 0);
    }

    return -1;
}

ParallelProfiler::ParallelProfiler(std::ostream& log, std::ostream& output)
    : PerfProfiler(log, output), m_status(ProfileStatus::ABORT) {}

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
    for (auto& [pid, config] : m_pidmap) {
        ok = (ok && killChild(pid));
    }
    return ok;
}

bool
ParallelProfiler::wakeupChild(pid_t pid, int signo) {
    if (!m_pidmap.count(pid)) { return false; }

    RunningConfig& config = m_pidmap.at(pid);

    // Process isn't stop, wake up failed.
    if (config.m_status != RunningConfig::STOP) { return false; }

    // Wake up child with signo.
    if (-1 == ptrace(PTRACE_CONT, pid, NULL, (void*)((long)signo))) { return false; }

    config.m_status = RunningConfig::RUN;
    
    m_log << "wake up plan[" << config.m_plan.getID() << "] done." << std::endl;

    return true;
}

bool
ParallelProfiler::wakeupAll() {
    for (auto& [pid, config] : m_pidmap) {
        if (RunningConfig::STOP == config.m_status) {
            if (!wakeupChild(pid, 0)) {
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
 * INIT:    Step phaseno when accept OVERFLOW_SIG, deliver other signal for child.
 *          A phase condition is satisfied when child phaseno matches its start point.
 *          If phaseno exceed its start point, we should abort.
 *          Step into PROFILE when all children meets their phase condition.
 * PROFILE: Step phaseno and collect data when accept OVERFLOW_SIG, deliver other signal for child.
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
        m_log << "error profile status[" << profStatus << "] while processing signal for child." << std::endl;
        return false;
    }

    while (-1 != (pid=waitpid(0, &status, WNOHANG))) {
        if (0 == pid) { break; }

        // This if clause guarantees that every processed pid is in m_pidmap.
        // So there is no need to check whether pid is valid in following processes, such as pid in m_pstatus.
        if (!m_pidmap.count(pid)) {
            m_log << "unknown pid[" << pid << "] while processing signal for child." << std::endl;
            setStatus(ProfileStatus::ABORT);
            break;
        }

        RunningConfig& config = m_pidmap.at(pid);
        const Plan& plan = config.m_plan;
        const TaskAttribute& task = plan.getTaskAttribute();

        m_log << "At profile status: " << profStatus << 
            " get plan: " << plan.getID() << 
            " signal: " << WSTOPSIG(status) << 
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
                m_log << "failed to start plan[" << plan.getID() << "]." << std::endl;
            } else if (ProfileStatus::INIT == profStatus) {
                // Child exit at INIT stage means the phase condition may be too large, abort.
                setStatus(ProfileStatus::ABORT);
                m_log << "plan[" <<  plan.getID() << "] exit at INIT stage." << std::endl;
            } else {
                // Child exit at PROFILE stage.
                // Try to step into DONE stage.
                if (!gotoDONE()) {
                    setStatus(ProfileStatus::ABORT);
                    m_log << "goto DONE stage failed at plan[" << plan.getID() << "] exit." << std::endl;
                    return true;
                }

                // Succeed to step into DONE stage.
                m_log << "plan[" <<  plan.getID() << "] exit normally." << std::endl;
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

            m_log << "plan[" << plan.getID() << "] abort by signal[" << WIFSIGNALED(status) << "]." << std::endl;
            
            // There is no need to process other children anymore(profile status may also be modified by other process).
            // So return immediately.
            return true;
        } else if (WIFSTOPPED(status)) {
            // Traced child receive a signal and stopped.
            // Collect the signal number.
            signo = WSTOPSIG(status);

            // Child stopped here, update status.
            config.m_status = RunningConfig::STOP;

            // Process signal according to profile status.
            switch (profStatus) {
            case READY:
                // Each traced child should start with SIGTRAP(triggered by exec).
                if (SIGTRAP == signo) {
                    // Child pid has reached READY stage, add it into m_pstatus.
                    m_pstatus[ProfileStatus::READY].insert(pid);

                    // The child should skip INIT stage either it doesn't have a phase condition,
                    // or its phase condition has been satisfied.
                    if (plan.getType() != Plan::Type::SAMPLE_PHASE || 0 == task.getPhaseBegin()) {
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
                            m_log << "goto PROFILE failed at plan[" << plan.getID() << "] in READY stage." << std::endl;
                            setStatus(ProfileStatus::ABORT);
                            return true;
                        }
                    } else {
                        // Some children need INIT stage to satisfy their phase condition.
                        if (!gotoINIT()) {
                            // If step into INIT stage failed then return false to report system error.
                            m_log << "goto INIT failed at plan[" << plan.getID() << "] in READY stage." << std::endl;
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
                // In INIT stage, OVERFLOW_SIG only send to the child who generates an overflow.
                // Only SAMPLE_PHASE plan can step into this stage.
                if (plan.getType() != Plan::Type::SAMPLE_PHASE) {
                    m_log << "plan[" << plan.getID() << "] shouldn't step into INIT stage." << std::endl;
                    setStatus(ProfileStatus::ABORT);
                    return true;
                }

                // Process signal for this child.
                if (ParallelProfiler::OVERFLOW_SIG == signo) {
                    // Receive OVERFLOW_SIG when an overflow occurs.

                    // Update phaseno for child.
                    if (!updatePhase(pid)) {
                        setStatus(ProfileStatus::ABORT);
                        m_log << "update phase failed for plan[" << plan.getID() << "] at INIT stage." << std::endl;
                        return true;
                    }

                    // Check whether phase condition is satisfied.
                    if (config.m_phaseno >= task.getPhaseBegin()) {
                        // Child finishs INIT stage, add it into m_pstate.
                        m_pstatus[ProfileStatus::INIT].insert(pid);
                    } else {
                        // Try to wake up this child for next phase.
                        // Return false to report system error if wake up failed.
                        if (!wakeupChild(pid, 0)) {
                            m_log << "failed to wakeup plan[" << plan.getID() << "] at INIT stage." << std::endl;
                            return false;
                        }
                    }
                } else {
                    // Child receive other signal, just deliver it.
                    if (!wakeupChild(pid, signo)) {
                        m_log << "failed to deliver signal[" << signo << "] to plan[" << plan.getID() << "]." << std::endl;
                        return false;
                    }
                }

                if (m_pidmap.size() == m_pstatus[ProfileStatus::INIT].size()) {
                    // INIT done, step into PROFILE stage.
                    if (!gotoPROFILE()) {
                        // If failed then return false to report system error.
                        m_log << "goto PROFILE failed at plan[" << plan.getID() << "] in INIT stage." << std::endl;
                        setStatus(ProfileStatus::ABORT);
                        return true;
                    }

                    // Terminate this handling to handle next stage.
                    return true;
                }

                // Process next child.
                break;

            case PROFILE:
                // In PROFILE stage, OVERFLOW_SIG will be send for group, which means all
                // children will recv OVERFLOW_SIG and deliver it to main process.
                if (ParallelProfiler::OVERFLOW_SIG == signo) {
                    // Use m_pstatus[PROFILE] to sync all children here.
                    m_pstatus[ProfileStatus::PROFILE].insert(pid);
                } else {
                    if (!wakeupChild(pid, signo)) {
                        m_log << "failed to deliver signal[" << signo << "] to plan[" << plan.getID() << "]." << std::endl;
                        return false;
                    }
                }

                if (m_pidmap.size() == m_pstatus[ProfileStatus::PROFILE].size()) {
                    // Sync done, all children have been stopped by OVERFLOW_SIG, reset m_pstatus to prepare for next sync.
                    m_pstatus[ProfileStatus::PROFILE].clear();

                    // Collect data for perf plan and update phase for sample plan.
                    if (!collectAll()) {
                        m_log << "collectAll failed at PROFILE stage." << std::endl;
                        setStatus(ParallelProfiler::ABORT);
                        return true;
                    }

                    // Check phase condition.
                    for (auto& [pid, config] : m_pidmap) {
                        const Plan& plan = config.m_plan;
                        const TaskAttribute& task = plan.getTaskAttribute();
                        if (Plan::Type::SAMPLE_PHASE == plan.getType() && config.m_phaseno >= task.getPhaseEnd()) {
                            m_pstatus[ProfileStatus::DONE].insert(pid);
                            m_log << "plan[" <<  plan.getID() << "] reaches phase end[" << 
                                task.getPhaseEnd() << "]." << std::endl;
                        }
                    }

                    if (!m_pstatus[ProfileStatus::DONE].empty()) {
                        // If any child meets its phase ending.
                        if (!gotoDONE()) {
                            setStatus(ProfileStatus::ABORT);
                            m_log << "goto DONE stage failed at plan[" << plan.getID() << "] exit." << std::endl;
                        }
                        return true;
                    } else {
                        // Wake up all children if no child meets phase ending.
                        if (!wakeupAll()) {
                            m_log << "wake up children failed at PROFILE stage." << std::endl;
                            return false;
                        }
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
        m_log << "read siginfo failed." << std::endl;
        return false;
    }

    m_log << "recv signal from pid[" << fdsi.ssi_pid << "] fd[" << fdsi.ssi_fd << 
        "] tid[" << fdsi.ssi_tid << "] signo[" << fdsi.ssi_signo << "]." << std::endl;

    switch (fdsi.ssi_signo) {
    case SIGINT:
        m_log << "receive SIGINT, stop profiling" << std::endl;
        setStatus(ProfileStatus::ABORT);
        break;
    case ParallelProfiler::OVERFLOW_SIG:
        break;
    case SIGCHLD:
        return handleChild(fdsi.ssi_pid);
    default:
        m_log << "Unhandled signal: " << fdsi.ssi_signo << std::endl;
        break;
    }

    return true;
}

int
ParallelProfiler::profile() {
    bool ok;
    pid_t ret;
    sample_t sum;
    struct pollfd pfd[1];
    std::vector<int> oldcpuset;
    int status, sfd = 0, err = 0;

    // Reset running config.
    m_pidmap.clear();
    m_result.clear();
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
        m_log << "init libpfm failed" << std::endl;
        err = -2;
        goto finalize;
    }

    if (-1 == (sfd=ParallelProfiler::createSignalFD())) {
        m_log << "create signal fd failed." << std::endl;
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

    // Ready to profile, wait for signal.
    m_log << "start profiling..." << std::endl;
    while (getStatus() < ProfileStatus::DONE) {
        pfd[0] = { sfd, POLLIN, 0 };
        if (-1 != poll(pfd, sizeof(pfd) / sizeof(*pfd), -1)) {
            if (pfd[0].revents & POLLIN){
                m_log << std::endl << "poll returns with POLLIN" << std::endl;
                if (!handleSignal(sfd)) {
                    setStatus(ProfileStatus::ABORT);
                    err = -errno;
                }
            }
        } else if (errno != EINTR) {
            m_log << "poll failed with errno: " << errno << std::endl;
            setStatus(ProfileStatus::ABORT);
            err = -errno;
        } else {
            m_log << "errno is EINTR" << std::endl;
        }
    }

    // TODO: remove this output when finish.
    // Profile done or abort as we get here.
    // Output perf record if the profile has done.
    if (ProfileStatus::DONE == getStatus()) {
        m_log << std::endl << "[output]" << std::endl;
        for (auto& [pid, config] : m_pidmap) {
            const Plan& plan = config.m_plan;
            auto& samples = config.m_samples;

            // Collect sum for perf plan.
            if (plan.perfPlan()) {
                sum = sample_t(config.m_event->GetChildNum()+1, 0);
                for (auto& sample : samples) {
                    for (size_t i=0; i<sample.size(); ++i) { sum[i] += sample[i]; }
                    m_result.emplace(plan.getID(), sum);
                }

                m_log << "sample for plan[" << plan.getID() << "] with phaseno[" << config.m_phaseno << "]." << std::endl;
                m_log << samples;
                m_log << "sum for plan[" << plan.getID() << "]: " << sum << std::endl;
            }
        }
    }

    err = getStatus();

terminate:
    killAll();
    while (-1 != (ret=waitpid(0, NULL, 0))) {
        m_log << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
            " stop by signal?: " << WIFSTOPPED(status) << 
            " terminated by signal?: " << WIFSIGNALED(status) << 
            " exit normally?: " << WIFEXITED(status) << std::endl;
    }

finalize:
    if (sfd > 0) { close(sfd); }
    pfm_terminate();
    m_cpuset = oldcpuset;

    return err;
}

bool
ParallelProfiler::authCheck() {
    if (0 != geteuid()) {
        m_log << "error: unprivileged user[" << getuid() << "]." << std::endl;
        return false;
    }
    return true;
}

bool
ParallelProfiler::argsCheck() {
    std::vector<int> validCPU;
    int nrNeededCPU, nrCPU = get_nprocs();

    // Collect valid cpu.
    for (auto cpuno : m_cpuset) {
        if (cpuno >= 0 && cpuno < nrCPU) {
            validCPU.emplace_back(cpuno);
        }
    }
    // Count needed cpu.
    nrNeededCPU = std::count_if(m_plan.begin(), m_plan.end(), 
        [] (const Plan& plan) -> bool { return plan.getTaskAttribute().needPinCPU(); }
    );

    // Check cpu.
    if (validCPU.size() < nrNeededCPU) {
        m_log << "nrValidCPU" << validCPU.size() << "] is smaller than nrNeededCPU[" << nrNeededCPU << "]." << std::endl;
        return false;
    }

    // Check plan.
    for (const auto& plan : m_plan) {
        if (!plan.valid()) {
            m_log << "plan[" << plan.getID() << "] is invalid." << std::endl;
            return false;
        }
    }

    // Swap current cpu set into valid cpu set.
    m_cpuset.swap(validCPU);

    return true;
}

bool
ParallelProfiler::updatePhase(pid_t pid) {
    std::vector<sample_t> samples;

    if (!m_pidmap.count(pid)) { return false; }

    RunningConfig& config = m_pidmap.at(pid);
    EventPtr event = config.m_event;
    const Plan& plan = config.m_plan;

    // Non-sample plan can not update phase.
    if (!plan.samplePlan()) { return false; }

    // Collect samples for this config and step phaseno.
    if (!collect(event, samples)) {
        m_log << "collect failed for plan[" << plan.getID() << "]." << std::endl;
        return false;
    }

    // Update phaseno.
    config.m_phaseno += samples.size();

    m_log << "update phase for plan[" << plan.getID() << "] with sample[" << samples.size() << "]:" << std::endl;
    m_log << samples;

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
        sample.clear();
        if (!collect(event, sample)) {
            // Collect failed, return false;
            m_log << "collect failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }
        samples.emplace_back(sample);

        // Update phase for sample plan.
        if (plan.samplePlan() && !updatePhase(pid)) { return false; }

        // Reset perf counter.
        try {
            event->Reset();
        } catch (PerfEventError e) {
            m_log << "failed to reset perf event for plan[" << plan.getID() << "]." << std::endl;
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
            m_log << "enable signal driven io failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }
        
        // Set file owner to child itself, cause INIT stage doesn't need to synchronize all children.
        if (!File::setFileOwner(fd, readyChild)) {
            // Failed to set file owner, return false to report system error.
            m_log << "set file owner failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }

        // Set signal while fd is readable.
        if (!File::setFileSignal(fd, ParallelProfiler::OVERFLOW_SIG)) {
            m_log << "set file signal failed for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }

        // Reset and Start the perf event.
        try {
            event->Reset();
            event->Start();
        } catch (PerfEventError e) {
            m_log << "failed to reset perf event for plan[" << plan.getID() << "]." << std::endl;
            return false;
        }

        // Wake up this child.
        if (!wakeupChild(readyChild, 0)) {
            m_log << "failed to wake up plan[" << plan.getID() << " when goto INIT." << std::endl;
            return false;
        }
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

        // Setup sample plan.
        // Note that we must re-enable signal driven io here.
        // Cause the SMAPLE_PHASE plan may not step into INIT stage if the phase start is zero.
        if (plan.samplePlan()) {
            // Get perf event fd.
            fd = event->GetFd();

            // Set signal driven IO for sample plan.
            if (!File::enableSigalDrivenIO(fd)) {
                // Failed to enable signal driven io, return false to report system error.
                m_log << "enable signal driven io failed for plan[" << plan.getID() << "]." << std::endl;
                return false;
            }
            
            // Set file owner to process group, so that all children can be synchronized by one OVERFLOW_SIG.
            if (!File::setFileOwner(fd, -gid)) {
                // Failed to set file owner, return false to report system error.
                m_log << "set file owner failed for plan[" << plan.getID() << "]." << std::endl;
                return false;
            }

            // Set signal while fd is readable.
            if (!File::setFileSignal(fd, ParallelProfiler::OVERFLOW_SIG)) {
                m_log << "set file signal failed for plan[" << plan.getID() << "]." << std::endl;
                return false;
            }
        }

        // Reset and Start perf event.
        if (plan.perfPlan()) {
            try {
                event->Reset();
                event->Start();
            } catch (PerfEventError e) {
                m_log << "failed to reset perf event for plan[" << plan.getID() << "]." << std::endl;
                return false;
            }
        }
    }

    // Wake up all children.
    if (!wakeupAll()) {
        m_log << "wake up children failed at READY stage." << std::endl;
        return false;
    }

    // All preparation done, step into PROFILE.
    setStatus(ProfileStatus::PROFILE);

    return true;
}

bool
ParallelProfiler::gotoDONE() {
    // Only PROFILE stage can step into DONE.
    if (ProfileStatus::PROFILE != getStatus()) { return false; }

    // Stop perf event for perf plan.
    try {
        for (auto& [pid, config] : m_pidmap) {
            if (config.m_plan.perfPlan()) {
                config.m_event->Stop();
            }
        }
    } catch (PerfEventError e) {
        m_log << "failed to stop perf event while gotoDONE." << std::endl;
        return false;
    }

    // Collect rest data and update phase for sample plan.
    if (!collectAll()) { return false; }
    
    // Collect & stop done, step into DONE.
    setStatus(ProfileStatus::DONE);

    return true;
}

bool
ParallelProfiler::buildRunningConfig(const Plan& plan) {
    int cpu = -1;
    pid_t pid = -1;
    EventPtr event = nullptr;
    RunningConfig conf(plan);

    const TaskAttribute& task = plan.getTaskAttribute();
    const PerfAttribute& perf = plan.getPerfAttribute();

    std::string cmd, var;
    std::vector<std::string> args, cmdVector;

    // Gen cmd args and create process.
    cmd = task.getTask().getCmd();
    args = task.getParam();
    if (!cmd.empty()) {
        for (int j=0; j<args.size(); ++j) {
            var = std::string(TaskAttribute::ARG_PREFIX) + std::to_string(j+TaskAttribute::ARG_INDEX_BEGIN);
            boost::replace_all(cmd, var, args[j]);
        }
        if (!cmd.empty()) {
            boost::split(cmdVector, cmd, boost::is_any_of(" "), boost::token_compress_on);
        }
    }
    
    // Start process.
    if (-1 == (pid=Process::start(std::bind(ParallelProfiler::setupSyncTask, task.getTask()), cmdVector))) { return false; }

    m_log << "start plan[" << plan.getID() << "] at pid[" << pid << "]." << std::endl;

    // Pin cpu for process.
    if (task.needPinCPU()) {
        if (m_cpuset.empty()) {
            m_log << "cpuset isn't enough for plan[" << plan.getID() << "]." << std::endl;
            goto killchild;
        }
        cpu = m_cpuset.back();
        if (!Process::setCPUAffinity(pid, cpu)) {
            m_log << "pin cpu failed for plan[" << plan.getID() << "] with errno[" << errno << "]." << std::endl;
            goto killchild;
        }
        m_cpuset.pop_back();
    }

    // Set rt process.
    if (task.isRT() && !Process::setFIFOProc(pid, sched_get_priority_max(SCHED_FIFO))) {
        m_log << "set fifo failed for plan[" << plan.getID() << "] with errno[" << errno << "]." << std::endl;
        goto killchild;
    }

    // Register perf event.
    if (plan.perfPlan()) {
        // Init perf event. Handle sample & count event in same step. 
        if (nullptr == (event=initEvent(plan.getPerfAttribute()))) {
            m_log << "register perf event failed  for plan[" << plan.getID() << "]." << std::endl;
            goto killchild;
        }

        // initEvent doesn't init pid, we should reset pid outside.
        event->SetTID(pid);

        // Default wake up is 1, but we shouldn't use wake up.
        // Otherwise the event will be stopped automatically after each overflow until a Refersh.
        event->SetWakeup(0);

        // Note that we should set read format explicit.
        // Cause the child event may be empty(Event object only set read format when invokes AttachChild).
        event->SetReadFormat(event->GetReadFormat() | PERF_FORMAT_GROUP);

        // As for sample plan, we have to set sample type to PERF_SAMPLE_READ.
        // Cause our collect function cannot handler other situation except PERF_SAMPLE_READ.
        if (plan.samplePlan()) { event->SetSampleType(PERF_SAMPLE_READ); }

        // Configure this event, and we can use Start/Stop/Reset/ProcessEvents to profile.
        try {
            event->Configure();
        } catch (PerfEventError e) {
            m_log << "failed to configure perf event for plan[" << plan.getID() << "]." << std::endl;
            goto killchild;
        }
    }

    // add running config
    conf.m_pid = pid;
    conf.m_cpu = cpu;
    conf.m_event = event;
    if (!m_pidmap.emplace(pid, conf).second) {
        m_log << "emplace running config failed for plan[" << plan.getID() << "]." << std::endl;
        goto killchild;
    }

    return true;

killchild:
    kill(pid, SIGKILL);
    return false;
}

//----------------------------------------------------------------------------//
