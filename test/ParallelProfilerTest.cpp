#include <iostream>
#include "ParallelProfiler.hpp"

using namespace std;

int main() {
    ParallelProfiler profiler(cout);
    Plan test(string(), Plan::Type::DAEMON, Task());
    Plan testDaemon(test);
    Plan testPhase(test);

    test.setID("test").setType(Plan::Type::DAEMON).setTask(Task("task", "/bin/ls $1"))
        .setParam({"-l"}).setRT(true).setPinCPU(true)
        .setPerfLeader("leader");
    testDaemon.setID("testDaemon").setType(Plan::Type::DAEMON).setTask(Task("daemon", "./TestDaemon"))
        .setRT(true)
        .setPerfLeader("leader");
    testPhase.setID("testPhase").setType(Plan::Type::COUNT).setTask(Task("daemon", "./TestDaemon"))
        .setRT(true)
        .setPerfLeader("leader").setPerfPeriod(1).setEnablePhase(true).setPhase({2, 5});

    profiler.addCPUSet(1);
    profiler.addPlan(test);
    profiler.addPlan(testDaemon);
    profiler.addPlan(testPhase);

    if (ParallelProfiler::ProfileStatus::DONE != profiler.profile()) {
        cout << "profile error with status: " << profiler.getStatus() << "." << endl;
    } else {
        cout << "profile done successfully." << endl;
    }
}
