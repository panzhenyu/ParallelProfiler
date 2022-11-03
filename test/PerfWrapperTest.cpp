#include <string>
#include <memory>
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/ptrace.h>
#include <sys/signalfd.h>
#include <boost/make_shared.hpp>
#include <perfmon/pfmlib_perf_event.h>
#include "PosixUtil.hpp"
#include "ConfigFactory.hpp"
#include "PerfEventWrapper.hpp"
#include "ParallelProfiler.hpp"

using namespace std;
using namespace Utils::Perf;
using namespace Utils::Posix;

int createSignalFD() {
    sigset_t mask;

    sigemptyset(&mask);
    // sigaddset(&mask, SIGINT);
    // sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, ParallelProfiler::OVERFLOW_SIG);

    if (-1 != sigprocmask(SIG_BLOCK, &mask, NULL)) {
        // We do not handle OVERFLOW_SIG, just ignore it.
        sigdelset(&mask, ParallelProfiler::OVERFLOW_SIG);
        return signalfd(-1, &mask, SFD_CLOEXEC);
    }

    return -1;
}

void handler(int signo) {
    if (SIGINT == signo) {
        kill(-getpgrp(), SIGKILL);
    }
}

int main() {
    bool c1, c2;
    sigset_t mask;
    int ret, status;
    pid_t child1, child2;
    PerfProfiler profiler(cout);
    PerfProfiler::sample_t sample;
    vector<PerfProfiler::sample_t> samples;
    PerfAttribute sampleINS = PerfAttributeFactory::generalPerfAttribute(
        "PERF_COUNT_HW_INSTRUCTIONS", 10000000, {"PERF_COUNT_HW_CPU_CYCLES"});

    if (PFM_SUCCESS != pfm_initialize()) {
        cout << "init libpfm failed." << endl;
        return -1;
    }

    if (-1 == createSignalFD()) {
        cout << "create signal fd failed." << endl;
        return -1;
    }

    child1 = fork();
    if (-1 == child1) {
        cout << "fork failed" << endl;
        goto out;
    } else if (0 == child1) {
        // in child1 process
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, ParallelProfiler::OVERFLOW_SIG);
        if (-1 == sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
            return -1;
        }
        Process::setCPUAffinity(0, 1);
        Process::setFIFOProc(0, sched_get_priority_max(SCHED_FIFO));
        ptrace(PTRACE_TRACEME);

        chdir("/usr/local/software/spec2006/benchspec/CPU2006/456.hmmer/run/run_base_ref_amd64-m64-gcc42-nn.0000/");
        assert(Process::exec({"./hmmer_base.amd64-m64-gcc42-nn", "nph3.hmm", "swiss41"}));

        cout << "child1 pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
        while (true);
    } else {
        child2 = fork();
        if (-1 == child2) {
            cout << "fork failed" << endl;
            goto out;
        } else if (0 == child2) {
            // in child2 process
            sigemptyset(&mask);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGCHLD);
            sigaddset(&mask, ParallelProfiler::OVERFLOW_SIG);
            if (-1 == sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
                return -1;
            }
            Process::setCPUAffinity(0, 2);
            Process::setFIFOProc(0, sched_get_priority_max(SCHED_FIFO));
            ptrace(PTRACE_TRACEME);

            chdir("/usr/local/software/spec2006/benchspec/CPU2006/403.gcc/run/run_base_ref_amd64-m64-gcc42-nn.0000/");
            assert(Process::exec({"./gcc_base.amd64-m64-gcc42-nn", "s04.i", "-o", "s04.s"}));

            cout << "child2 pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
            while (true);
        } else {
            cout << "parent pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
            signal(SIGINT, handler);

            /* configure for event */
            auto event1 = profiler.initEvent(sampleINS);
            auto event2 = profiler.initEvent(sampleINS);

            event1->SetTID(child1);
            event1->SetWakeup(0);
            event1->SetReadFormat(event1->GetReadFormat() | PERF_FORMAT_GROUP);
            event1->SetSampleType(PERF_SAMPLE_READ);
            event1->Configure();

            event2->SetTID(child2);
            event2->SetWakeup(0);
            event2->SetReadFormat(event2->GetReadFormat() | PERF_FORMAT_GROUP);
            event2->SetSampleType(PERF_SAMPLE_READ);
            event2->Configure();

            assert(File::enableSigalDrivenIO(event1->GetFd()));
            assert(File::setFileOwner(event1->GetFd(), -getpgrp()));
            assert(File::setFileSignal(event1->GetFd(), ParallelProfiler::OVERFLOW_SIG));

            assert(File::enableSigalDrivenIO(event2->GetFd()));
            assert(File::setFileOwner(event2->GetFd(), -getpgrp()));
            assert(File::setFileSignal(event2->GetFd(), ParallelProfiler::OVERFLOW_SIG));

            event1->Start();
            event2->Start();

            c1 = c2 = false;

            cout << "event started." << endl << endl;
            while (-1 != (ret=waitpid(-1, &status, 0))) {
                std::cout << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
                    " stop by signal?: " << WIFSTOPPED(status) << 
                    " terminated by signal?: " << WIFSIGNALED(status) << 
                    " exit normally?: " << WIFEXITED(status) << std::endl;

                if (ret == child1) { c1 = true; }
                if (ret == child2) { c2 = true; }

                if (!c1 || !c2) { continue; }

                c1 = c2 = false;

                // samples.clear();
                // if (profiler.collect(event1, samples)) {
                //     cout << "size of samples1[" << samples.size() << "]." << endl;
                //     for (auto& sample : samples) {
                //         for (auto d : sample) {
                //             cout << d << " ";
                //         }
                //         cout << endl;
                //     }
                // } else {
                //     cout << "collect failed" << endl;
                //     break;
                // }

                // samples.clear();
                // if (profiler.collect(event2, samples)) {
                //     cout << "size of samples2[" << samples.size() << "]." << endl;
                //     for (auto& sample : samples) {
                //         for (auto d : sample) {
                //             cout << d << " ";
                //         }
                //         cout << endl;
                //     }
                // } else {
                //     cout << "collect failed" << endl;
                //     break;
                // }

                sample.clear();
                if (profiler.collect(event1, sample)) {
                    cout << "size of sample1[" << sample.size() << "]." << endl;
                    for (auto d : sample) {
                        cout << d << " ";
                    }
                    cout << endl;
                } else {
                    cout << "collect failed" << endl;
                    break;
                }

                sample.clear();
                if (profiler.collect(event2, sample)) {
                    cout << "size of sample2[" << sample.size() << "]." << endl;
                    for (auto d : sample) {
                        cout << d << " ";
                    }
                    cout << endl;
                } else {
                    cout << "collect failed" << endl;
                    break;
                }
                cout << endl;
                
                // event1->Reset();
                // event2->Reset();

                // sleep(1);

                assert (-1 != ptrace(PTRACE_CONT, child2, 0, 0));
                assert (-1 != ptrace(PTRACE_CONT, child1, 0, 0));
            }
            cout << errno << endl;
        }
    }

out:
    pfm_terminate();
    kill(-getpgrp(), SIGKILL);
    return 0;
}
