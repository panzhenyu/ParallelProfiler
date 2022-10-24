#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include "Config.hpp"
#include "ConfigFactory.hpp"
#include "rapidjson/reader.h"
#include "ParallelProfiler.hpp"
#include "rapidjson/document.h"

using namespace std;
namespace po = boost::program_options;

/**
 * [Usage]
 *      sudo ./ParallelProfile
 *          --config                            Optional    file path, such as conf/example.json, default is empty
 *          --output                            Optional    file path, default is stdout
 *          --log                               Optional    file path, default is stderr
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

enum ErrCode {
    UNKNOWN_OPT=1,
    INVALID_CPU,
    ZERO_PLAN,
    CONFLICT_CPU,
    CONFLICT_PLAN,
};

#define ERR     cout << "[ERROR] "
#define INFO    cout << "[INFO]  "

struct ProfilerArguments {
public:
    void parse(int argc, char* argv[]) {
        po::options_description desc("ParallelProfile [--options]");
        po::variables_map vm;
        string cpu;

        desc.add_options()
            ("config", po::value<string>(&m_config)->default_value(string()), "config file")
            ("output", po::value<string>(&m_output)->default_value(string()), "output file")
            ("log", po::value<string>(&m_log)->default_value(string()), "log file")
            ("cpu", po::value<string>(&cpu)->default_value(string()), "cpuset used for plan, such as 1,2~4")
            ("plan", po::value<vector<string>>(&m_plan)->multitoken(), "plan to profile, must provide one plan at least")
            ("help", "show this message");

        try {
            po::store(po::parse_command_line(argc, argv, desc), vm);
            po::notify(vm);
        } catch (...) {
            ERR << "unrecognized option exits." << endl;
            exit(-UNKNOWN_OPT);
        }

        if (vm.count("help")) {
            cout << desc << endl;
            exit(0);
        } else {
            if (!cpu.empty()) {
                int pos, begin, end;
                vector<string> cpusetSplit;

                // Case: --cpu=1,2~4
                boost::split(cpusetSplit, cpu, boost::is_any_of(","), boost::token_compress_on);

                // Handle each cpuset.
                try {
                    for (auto& cpuset : cpusetSplit) {
                        if (string::npos == (pos=cpuset.find("~"))) {
                            // Pattern: 1
                            m_cpu.emplace_back(boost::lexical_cast<int>(cpuset));
                        } else {
                            // Pattern: 2~4
                            begin = boost::lexical_cast<int>(cpuset.substr(0, pos));
                            end = boost::lexical_cast<int>(cpuset.substr(pos+1));

                            // Add 2, 3, 4 to m_cpu.
                            for (int i=begin; i<=end; ++i) {
                                m_cpu.emplace_back(i);
                            }
                        }
                    }
                } catch (...) {
                    ERR << "invalid argument [--cpu=" << cpu << "]." << endl;
                    exit(-INVALID_CPU);
                }
            }
            // CPU no cannot repeat.
            sort(m_cpu.begin(), m_cpu.end());
            for (int i=0; i<m_cpu.size()-1; ++i) {
                if (m_cpu[i] == m_cpu[i+1]) {
                    ERR << "conflict cpuno[" << m_cpu[i] << "]." << endl;
                    exit(-CONFLICT_CPU);
                }
            }

            if (m_plan.empty()) {
                ERR << "invalid plan num[" << m_plan.size() << "], provide one plan at least" << "." << endl;
                exit(-ZERO_PLAN);
            }
        }
    }

public:
    string              m_config;
    string              m_output;
    string              m_log;
    vector<int>         m_cpu;
    vector<string>      m_plan;
};

Plan parseFromString(const string& planStr, const rapidjson::Document& config) {
    return PlanFactory::defaultPlan("plan", Plan::Type::DAEMON);
}

int main(int argc, char *argv[]) {
    ProfilerArguments args;
    rapidjson::Document config;
    ofstream outfile, logfile;
    ostream *output, *log;

    args.parse(argc, argv);

    // Get json doc for config.
    config.Parse(args.m_config.c_str());

    // Get output stream.
    if (!args.m_output.empty()) {
        outfile = ofstream(args.m_output.c_str(), std::ofstream::out);
        output = &outfile;
    } else {
        output = &cout;
    }

    // Get log stream.
    if (!args.m_log.empty()) {
        logfile = ofstream(args.m_log.c_str(), std::ofstream::out);
        output = &logfile;
    } else {
        output = &cerr;
    }

    // CPU set has already been parsed.

    // Build profiler.
    ParallelProfiler profiler(*log, *output);
    profiler.setCPUSet(args.m_cpu);

    // Parse and add plan.
    unordered_set<string> planid;
    for (const string& planStr : args.m_plan) {
        Plan plan = parseFromString(planStr, config);
        if (planid.count(plan.getID())) {
            ERR << "conflict planid[" << plan.getID() << "]." << endl;
            exit(-CONFLICT_PLAN);
        }
        profiler.addPlan(plan);
        planid.insert(plan.getID());
    }

    // Log helper info before profile.
    INFO << "Start parallel profiling with setting:" << endl;
    INFO << "[Config] " << args.m_config << endl;
    INFO << "[Output] " << (args.m_output.empty() ? "stdout" : args.m_output) << endl;
    INFO << "[Log] " << (args.m_log.empty() ? "stdout" : args.m_log) << endl;
    INFO << "[CPUSet] ";
    for (auto cpu : args.m_cpu) {
        cout << cpu << ",";
    }
    cout << endl;
    INFO << "[Plan] ";
    for (auto id : planid) {
        cout << id << ",";
    }

    return 0;
}
