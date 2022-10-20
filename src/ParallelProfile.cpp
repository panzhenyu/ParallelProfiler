#include <vector>
#include <string>
#include "Config.hpp"
#include "cmdline.hpp"

/**
 * [Usage]
 *      sudo ./ParallelProfile
 *          --task-conf                         Required    path for file, such as conf/task.json
 *          --plan-conf                         Required    path for file, such as conf/plan.json
 *          --output                            Optional    path for file, default is stdout
 *          --log                               Optional    path for file, default is stderr
 *          --cpu                               Optional    such as 1,2~4, default is empty
 *          --plan                              Repeated    such as id or "{key:value[,key:value]}", at least one plan
 * [Supported Key]
 *          id                                  Required    such as "myplan"
*           task                                Required    such as "./task"
 *          type                                Required    choose "DAEMON" or "COUNT" or "SAMPLE_ALL" or "SAMPLE_PHASE"
 *          rt                                  Optional    choose true or false, default is false
 *          pincpu                              Optional    choose true or false, default is false
 *          phase                               Optional    such as [start,end], default is [0,0]
 *          perf-leader                         Optional    such as "INSTURCTIONS", default is empty
 *          sample-period                       Optional    default is 0
 *          perf-member                         Optional    such as [MEMBER1, MEMBER2], default is empty
 */
struct ProfilerArguments {
public:
    bool parse(int argc, char* argv[]) {

    }

public:
    std::string                 m_planConfig;
    std::string                 m_taskConfig;
    std::vector<std::string>    m_plan;
    std::vector<int>            m_cpu;
    std::string                 m_output;
};

int main(int argc, char *argv[]) {
    ProfilerArguments args;
    if (!args.parse(argc, argv)) {
        return -1;
    }
    return 0;
}
