#include <vector>
#include <string>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include "Config.hpp"
#include "rapidjson/reader.h"
#include "rapidjson/rapidjson.h"

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
            cout << "unrecognized option exits." << endl;
            exit(-1);
        }

        if (vm.count("help")) {
            cout << desc << endl;
            exit(0);
        } else {
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
                cout << "invalid argument [--cpu=" << cpu << "]." << endl;
                exit(-2);
            }

            if (m_plan.empty()) {
                cout << "invalid plan num[" << m_plan.size() << "], provide one plan at least" << "." << endl;
                exit(-3);
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

int main(int argc, char *argv[]) {
    ProfilerArguments args;
    args.parse(argc, argv);
    return 0;
}
