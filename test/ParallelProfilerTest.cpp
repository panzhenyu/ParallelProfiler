#include <iostream>
#include "ParallelProfiler.hpp"

using namespace std;

int main() {
    ParallelProfiler profiler(cout);
    Plan test;

    test.setID("test").serPerfLeader("leader").setRT(true).setPinCPU(true)
        .setTask(Task("task", "/bin/ls $1")).setParam({"-l"});
    profiler.addCPUSet(1);
    profiler.addPlan(test);
    cout << profiler.profile() << endl;
}
