#include <iostream>
#include "ParallelProfiler.hpp"
#include "ConfigFactory.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    ParallelProfiler profiler(cout, cout);
    Task ls = TaskFactory::buildTask("ls", "/bin/ls $1");
    Task daemon = TaskFactory::buildTask("daemon", "./TestDaemon");

    TaskAttribute normalLS = TaskAttributeFactory::generalTaskAttribute(ls, {"-la"}, true, true, 0, 0);
    TaskAttribute normalDaemon = TaskAttributeFactory::normalTaskAttribute(daemon, {}, true, true);
    TaskAttribute phaseDaemon = TaskAttributeFactory::generalTaskAttribute(daemon, {}, true, true, 2, 5);

    PerfAttribute sampleINS = PerfAttributeFactory::simpleSamplePerfAttribute("INSTRUCTIONS", 800000);

    Plan test = PlanFactory::daemonPlan("test", normalLS);
    Plan testDaemon = PlanFactory::daemonPlan("testDaemon", normalDaemon);
    Plan testPhase = PlanFactory::generalPlan("testPhase", Plan::Type::SAMPLE_PHASE, phaseDaemon, sampleINS);

    // test.setID("test").setType(Plan::Type::DAEMON).setTask(Task("task", "/bin/ls $1"))
    //     .setParam({"-l"}).setRT(true).setPinCPU(true)
    //     .setPerfLeader("leader");
    // testDaemon.setID("testDaemon").setType(Plan::Type::DAEMON).setTask(Task("daemon", "./TestDaemon"))
    //     .setRT(true)
    //     .setPerfLeader("leader");
    // testPhase.setID("testPhase").setType(Plan::Type::DAEMON).setTask(Task("daemon", "./TestDaemon"))
    //     .setRT(true)
    //     .setPerfLeader("leader").setPerfPeriod(1).setPhase({2, 5});

    profiler.addCPUSet({1,2,3});
    profiler.addPlan(test);
    profiler.addPlan(testDaemon);
    profiler.addPlan(testPhase);

    if (ParallelProfiler::ProfileStatus::DONE != profiler.profile()) {
        cout << "profile error with status: " << profiler.getStatus() << "." << endl;
    } else {
        cout << "profile done successfully." << endl;
    }
}
