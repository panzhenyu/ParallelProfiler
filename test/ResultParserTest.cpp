#include "ResultParser.hpp"
#include "ConfigFactory.hpp"
#include <iostream>

using namespace std;

int main() {
    ResultParser parser;
    ParallelProfiler::result_t result;
    std::vector<std::vector<uint64_t>> data = {{1,2,3}, {3,4,5}};

    cout << parser.parseFile("out") << endl;
    result.emplace_back(PlanFactory::generalPlan("plan1", Plan::Type::DAEMON, TaskAttributeFactory::defaultTaskAttribute(), 
        PerfAttributeFactory::generalPerfAttribute("leader1", 100, {"e10", "e20"})), data);
    result.emplace_back(PlanFactory::generalPlan("plan2", Plan::Type::DAEMON, TaskAttributeFactory::defaultTaskAttribute(), 
        PerfAttributeFactory::generalPerfAttribute("leader2", 100, {"e11", "e21"})), data);
    
    cout << parser.append(result) << endl;
    cout << parser.json() << endl;

    return 0;
}