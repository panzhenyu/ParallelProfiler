#include <vector>
#include <string>
#include "Config.hpp"
#include "cmdline.hpp"

/**
 * [Usage]
 *      sudo ./ParallelProfile
 *          --task-conf=conf/task.json                          Required
 *          --plan-conf=conf/plan.json                          Required
 *          --cpu=1,2~4                                         Optional, default is empty
 *          --output=                                           Optional, default is stdout
 *          --log=                                              Optional, default is stderr
 *          --plan=Set2024 --plan=Set2036                       At least one plan
 *          --plan="key=value[&&key=value]"                     Optional(support diy plan without configuration)
 *              [keys]
 *              type=DAEMON//COUNT/SAMPLE_ALL/SAMPLE_PHASE      Required
 *              task='cmd'                                      Required
 *              rt=true/false                                   Optional, default is false
 *              pincpu=true/false                               Optional, default is false
 *              phase=start,end                                 Optional, default is 0,0
 *              perf-leader=                                    Optional, default is empty string
 *              sample-period=                                  Optional, default is 0
 *              perf-member=MEMBER[,MEMBER]                     Optional, default is empty member
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
