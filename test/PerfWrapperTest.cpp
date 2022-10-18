#include <string>
#include <memory>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/ptrace.h>
#include <perfmon/pfmlib_perf_event.h>
#include <boost/make_shared.hpp>
#include "PosixUtil.hpp"
#include "PerfEventWrapper.hpp"
#include "ParallelProfiler.hpp"

using namespace std;
using namespace Utils::Perf;
using namespace Utils::Posix;

pid_t pid;

void handler(int signo) {
    if (SIGINT == signo) {
        kill(-getpgrp(), SIGKILL);
    }
}

struct ProfilerSample {
    std::vector<uint64_t> data;
    int nrEvent;
};

void collect(Event* e, void* v, int status) {
    uint64_t val;
    ProfilerSample* sample = static_cast<ProfilerSample*>(v);
    if (status) return;
    for (int i=0; i<sample->nrEvent; ++i) {
        // read nr
        val = e->Read<uint64_t>();
        cout << "nr[" << val << "]." << endl;
        // read value
        val = e->Read<uint64_t>();
        cout << "value1[" << val << "]." << endl;
        // read id
        val = e->Read<uint64_t>();
        cout << "value2[" << val << "]." << endl;
    }
}

int main() {
    int ret, status;
    Event *e;
    string event = "INSTRUCTIONS";
    PerfEventEncode encode;
    PerfProfiler profiler(cout, cout);
    PerfProfiler::sample_t record;
    std::vector<PerfProfiler::sample_t> samples;


    if (PFM_SUCCESS != pfm_initialize()) {
        cout << "init libpfm failed." << endl;
        return -1;
    }

    if (!getPerfEventEncoding(event, encode)) {
        cout << "get encoding for" << event << endl;
        return -2;
    }

    pid = fork();
    if (-1 == pid) {
        cout << "fork failed" << endl;
        goto out;
    } else if (0 == pid) {
        // in child process
        ptrace(PTRACE_TRACEME);
        cout << "child pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
        while (true);
    } else {
        cout << "parent pid[" << getpid() << "] gid[" << getgid() << "]" << endl;
        signal(SIGINT, handler);
        signal(SIGIO, handler);

        /* configure for event */
        e = new Event(pid, encode.config, encode.type, 10000000, PERF_SAMPLE_READ);
        e->AttachEvent(boost::make_shared<ChildEvent>(0, PERF_TYPE_HARDWARE));
        e->SetPreciseIp();
        e->SetWakeup(1);
        e->EnableSigIO();
        e->Configure();
        e->Reset();
        e->Start();

        // assert(Event::CONFIGURED == e->GetState());
        assert(true == File::setFileOwner(e->GetFd(), -getpgrp()));
        assert(true == File::enableSigalDrivenIO(e->GetFd()));

        EventPtr event(e);
        cout << "event started." << endl;
        // while (true) {
        //     record.clear();
        //     if (profiler.collect(event, record)) {
        //         cout << "sizeof record[" << record.size() << "]." << endl;
        //         for (auto& d : record) { cout << d << " "; }
        //         cout << endl;
        //     } else {
        //         cout << "collect failed" << endl;
        //         break;
        //     }
        //     event->Reset();
        //     // sleep(1);
        //     ptrace(PTRACE_CONT, pid, NULL, NULL);
        // }
        while (-1 != (ret=waitpid(-1, &status, 0))) {
            std::cout << "get pid: " << ret << " signal: " << WSTOPSIG(status) << 
                " stop by signal?: " << WIFSTOPPED(status) << 
                " terminated by signal?: " << WIFSIGNALED(status) << 
                " exit normally?: " << WIFEXITED(status) << std::endl;

            samples.clear();
            if (profiler.collect(event, samples)) {
                cout << "size of samples[" << samples.size() << "]." << endl;
                for (auto& sample : samples) {
                    for (auto d : sample) {
                        cout << d << " ";
                    }
                    cout << endl;
                }
            } else {
                cout << "collect failed" << endl;
                break;
            }
            sleep(1);

            event->Reset();
            event->Refresh();
            ptrace(PTRACE_CONT, pid, NULL, NULL);
        }
        kill(pid, SIGKILL);
    }

out:
    pfm_terminate();
    return 0;
}