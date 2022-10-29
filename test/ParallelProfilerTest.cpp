#include <iostream>
#include "ParallelProfiler.hpp"
#include "ConfigFactory.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    ParallelProfiler profiler(cout);
    Task ls = TaskFactory::buildTask("ls", "/bin/ls $1");
    Task daemon = TaskFactory::buildTask("daemon", "./TestDaemon");
    Task mcf = TaskFactory::buildTask("mcf", "./mcf_r_base.mytest-m64 inp.in", 
        "/usr/local/software/spec2017/benchspec/CPU/505.mcf_r/run/run_base_refrate_mytest-m64.0000");

    TaskAttribute normalLS = TaskAttributeFactory::normalTaskAttribute(ls, {"-la"}, true, true);
    TaskAttribute normalDaemon = TaskAttributeFactory::normalTaskAttribute(daemon, {}, true, true);
    TaskAttribute phaseMCF = TaskAttributeFactory::generalTaskAttribute(mcf, {}, true, true, 0, 5);
    TaskAttribute phaseDaemon1 = TaskAttributeFactory::generalTaskAttribute(daemon, {}, true, true, 5, 9);
    TaskAttribute phaseDaemon2 = TaskAttributeFactory::generalTaskAttribute(daemon, {}, true, true, 0, 5);

    PerfAttribute sampleINS = PerfAttributeFactory::generalPerfAttribute(
        "PERF_COUNT_HW_INSTRUCTIONS", 500000000, {"PERF_COUNT_HW_CPU_CYCLES", "LLC_MISSES"});

    Plan test = PlanFactory::generalPlan("test", Plan::Type::COUNT, normalLS, sampleINS);
    Plan testDaemon = PlanFactory::generalPlan("testDaemon", Plan::Type::SAMPLE_ALL, normalDaemon, sampleINS);
    Plan mcfplan = PlanFactory::generalPlan("mcf", Plan::Type::SAMPLE_PHASE, phaseMCF, sampleINS);
    Plan testPhase1 = PlanFactory::generalPlan("testPhase1", Plan::Type::SAMPLE_PHASE, phaseDaemon1, sampleINS);
    Plan testPhase2 = PlanFactory::generalPlan("testPhase2", Plan::Type::SAMPLE_PHASE, phaseDaemon2, sampleINS);

    // test.setID("test").setType(Plan::Type::DAEMON).setTask(Task("task", "/bin/ls $1"))
    //     .setParam({"-l"}).setRT(true).setPinCPU(true)
    //     .setPerfLeader("leader");
    // testDaemon.setID("testDaemon").setType(Plan::Type::DAEMON).setTask(Task("daemon", "./TestDaemon"))
    //     .setRT(true)
    //     .setPerfLeader("leader");
    // testPhase.setID("testPhase").setType(Plan::Type::DAEMON).setTask(Task("daemon", "./TestDaemon"))
    //     .setRT(true)
    //     .setPerfLeader("leader").setPerfPeriod(1).setPhase({2, 5});

    profiler.addCPUSet({3, 4});
    // profiler.addPlan(test);
    // profiler.addPlan(testDaemon);
    // profiler.addPlan(mcfplan);
    // profiler.addPlan(testPhase1);
    profiler.addPlan(testPhase2);

    if (ParallelProfiler::ProfileStatus::DONE != profiler.profile()) {
        cout << "profile error with status: " << profiler.getStatus() << "." << endl;
    } else {
        cout << "profile done successfully." << endl;
    }
}
