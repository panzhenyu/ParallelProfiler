#include <iostream>
#include "ParallelProfiler.hpp"

using namespace std;

int main() {
    ParallelProfiler profiler(cout);
    Plan test, testDaemon, testPhase;

    test.setID("test").setPerfLeader("leader").setRT(true).setPinCPU(true).setEnablePhase(false)
        .setTask(Task("task", "/bin/ls $1")).setParam({"-l"});
    testDaemon.setID("testDaemon").setPerfLeader("leader").setRT(true).setEnablePhase(false)
        .setTask(Task("daemon", "./TestDaemon"));
    testPhase.setID("testPhase").setPerfLeader("leader").setPerfPeriod(1).setEnablePhase(true).setPhase({2, 5}).setRT(true)
        .setTask(Task("daemon", "./TestDaemon"));

    profiler.addCPUSet(1);
    profiler.addPlan(test);
    profiler.addPlan(testDaemon);
    // profiler.addPlan(testPhase);

    while (true) {
        if (ParallelProfiler::ProfileStatus::DONE != profiler.profile()) {
            cout << "profile error with status: " << profiler.getStatus() << endl;
            break;
        }
    }
}
