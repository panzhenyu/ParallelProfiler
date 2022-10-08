#include <vector>
#include <string>
#include "Config.hpp"
#include "cmdline.hpp"

/**
 * usage: sudo ./profiler --task-conf=conf/task.json --plan-conf=conf/plan.json --plan=Set2024 --plan=Set2036 --cpu=1,2~4 --output=
 * when cpu is set and number of cpu < the number of plan, abort!
 * when plan is invalid, abort!
 */
class ProfilerArguments {
public:
    bool parse(int, char*[]);
private:
    std::string                 m_planConfig;
    std::string                 m_taskConfig;
    std::vector<std::string>    m_plan;
    std::vector<int>            m_cpu;
    std::string                 m_output;
};

int main(int argc, char *argv[]) {
}
