#include <string>
#include <memory>
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/ptrace.h>
#include <perfmon/pfmlib_perf_event.h>
#include <boost/make_shared.hpp>
#include "PosixUtil.hpp"
#include "ConfigFactory.hpp"
#include "PerfEventWrapper.hpp"
#include "ParallelProfiler.hpp"

using namespace std;
using namespace Utils::Perf;
using namespace Utils::Posix;

void handler(int signo) {
    if (SIGINT == signo) {
        kill(-getpgrp(), SIGKILL);
    }
}

int main() {
    bool c1, c2;
    int ret, status;
    pid_t child1, child2;
    PerfProfiler profiler(cout);
    PerfProfiler::sample_t sample;
    vector<PerfProfiler::sample_t> samples;
    PerfAttribute sampleINS = PerfAttributeFactory::generalPerfAttribute(
        "PERF_COUNT_HW_INSTRUCTIONS", 100000, {"PERF_COUNT_HW_CPU_CYCLES"});

    if (PFM_SUCCESS != pfm_initialize()) {
        cout << "init libpfm failed." << endl;
        return -1;
    }

    child1 = fork();
    if (-1 == child1) {
        cout << "fork failed" << endl;
        goto out;
    } else if (0 == child1) {
        // in child1 process
        ptrace(PTRACE_TRACEME);
        cout << "child1 pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
        while (true);
    } else {
        child2 = fork();
        if (-1 == child2) {
            cout << "fork failed" << endl;
            goto out;
        } else if (0 == child2) {
            // in child2 process
            ptrace(PTRACE_TRACEME);
            cout << "child2 pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
            while (true);
        } else {
            cout << "parent pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
            signal(SIGINT, handler);
            signal(SIGIO, handler);

            /* configure for event */
            auto event1 = profiler.initEvent(sampleINS);
            auto event2 = profiler.initEvent(sampleINS);

            event1->SetTID(child1);
            event1->SetWakeup(0);
            event1->SetSampleType(PERF_SAMPLE_READ);
            event1->Configure();

            event2->SetTID(child2);
            event2->SetWakeup(0);
            event2->SetSampleType(PERF_SAMPLE_READ);
            event2->Configure();

            assert(true == File::enableSigalDrivenIO(event1->GetFd()));
            assert(true == File::setFileOwner(event1->GetFd(), -getpgrp()));

            assert(true == File::enableSigalDrivenIO(event2->GetFd()));
            assert(true == File::setFileOwner(event2->GetFd(), -getpgrp()));

            event1->Start();
            event2->Start();

            c1 = c2 = false;

            cout << "event started." << endl << endl;
            while (-1 != (ret=waitpid(-1, &status, 0))) {
                std::cout << endl << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
                    " stop by signal?: " << WIFSTOPPED(status) << 
                    " terminated by signal?: " << WIFSIGNALED(status) << 
                    " exit normally?: " << WIFEXITED(status) << std::endl;

                if (ret == child1) { c1 = true; }
                if (ret == child2) { c2 = true; }
                if (!c1 || !c2) { continue; }
                while (-1 != (ret=waitpid(-1, &status, WNOHANG))) {
                    if (0 == ret) { break; }
                }
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
                    cout << "size of sample1[" << samples.size() << "]." << endl;
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
                    cout << "size of sample2[" << samples.size() << "]." << endl;
                    for (auto d : sample) {
                        cout << d << " ";
                    }
                    cout << endl;
                } else {
                    cout << "collect failed" << endl;
                    break;
                }

                assert (-1 != ptrace(PTRACE_CONT, child1, 0, 0));
                assert (-1 != ptrace(PTRACE_CONT, child2, 0, 0));
            }
            cout << errno << endl;
        }
    }

out:
    pfm_terminate();
    kill(-getpgrp(), SIGKILL);
    return 0;
}
