#include <iostream>
#include "ParallelProfiler.hpp"

using namespace std;

int main() {
    ParallelProfiler profiler(cout);
    Plan test, testDaemon;

    test.setID("test").setPerfLeader("leader").setRT(true).setPinCPU(true).setEnablePhase(false)
        .setTask(Task("task", "/bin/ls $1")).setParam({"-l"});
    testDaemon.setID("testDaemon").setPerfLeader("leader").setRT(true).setEnablePhase(false)
        .setTask(Task("daemon", "./TestDaemon"));
    profiler.addCPUSet(1);
    profiler.addPlan(test);
    profiler.addPlan(testDaemon);
    cout << profiler.profile() << endl;
}
