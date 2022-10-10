#include <iostream>
#include "ParallelProfiler.hpp"

using namespace std;

int main() {
    ParallelProfiler profiler(cout);
    Plan test, testDaemon;

    test.setID("test").serPerfLeader("leader").setRT(true).setPinCPU(true)
        .setTask(Task("task", "/bin/ls $1")).setParam({"-l"});
    testDaemon.setID("testDaemon").serPerfLeader("leader").setRT(true)
        .setTask(Task("daemon", "./TestDaemon"));
    profiler.addCPUSet(1);
    profiler.addPlan(test);
    // profiler.addPlan(testDaemon);
    cout << profiler.profile() << endl;
}
