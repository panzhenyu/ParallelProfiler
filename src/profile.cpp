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
#include "ConfigParser.hpp"
#include "ConfigFactory.hpp"
#include "ParallelProfiler.hpp"

using namespace std;
namespace po = boost::program_options;

static char* helpmsg = (char*)"\
[Usage]                                                                                                         \n\
    sudo ./profile                                                                                              \n\
        --config        Optional        file path, such as conf/example.json, default is empty                  \n\
        --output        Optional        file path, default is stdout                                            \n\
        --log           Optional        file path, default is stderr                                            \n\
        --cpu           Optional        such as 1,2~4, default is empty                                         \n\
        --plan          Repeated        such as id or \"{key:value[,key:value]}\", at least one plan            \n\
[Supported Key]                                                                                                 \n\
        id              Required        such as \"myplan\"                                                      \n\
        task            Required        such as \"./task\"                                                      \n\
        type            Required        choose \"DAEMON\" or \"COUNT\" or \"SAMPLE_ALL\" or \"SAMPLE_PHASE\"    \n\
        rt              Optional        choose true or false, default is false                                  \n\
        pincpu          Optional        choose true or false, default is false                                  \n\
        phase           Optional        such as [start,end], default is [0,0]                                   \n\
        leader          Optional        such as \"INSTURCTIONS\", default is empty                              \n\
        period          Optional        default is 0                                                            \n\
        member          Optional        such as [MEMBER1, MEMBER2], default is empty                            \
";

#define ERRCODE -1

#define ERR     cout << "[ERROR] "
#define INFO    cout << "[INFO]  "

struct ProfilerArguments {
public:
    void parse(int argc, char* argv[]) {
        po::options_description desc("profile [--options]");
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
            cout << helpmsg << endl;
            exit(ERRCODE);
        }

        if (vm.count("help")) {
            cout << helpmsg << endl;
            exit(0);
        } else {
            if (!cpu.empty()) {
                int pos, begin, end;
                unordered_set<int> cpuno;
                vector<string> cpusetSplit;

                // Case: --cpu=1,2~4
                boost::split(cpusetSplit, cpu, boost::is_any_of(","), boost::token_compress_on);

                // Handle each cpuset.
                try {
                    for (auto& cpuset : cpusetSplit) {
                        if (string::npos == (pos=cpuset.find("~"))) {
                            // Pattern: 1
                            cpuno.insert(boost::lexical_cast<int>(cpuset));
                        } else {
                            // Pattern: 2~4
                            begin = boost::lexical_cast<int>(cpuset.substr(0, pos));
                            end = boost::lexical_cast<int>(cpuset.substr(pos+1));

                            // Add 2, 3, 4 to m_cpu.
                            for (int i=begin; i<=end; ++i) { cpuno.insert(i); }
                        }
                    }
                } catch (...) {
                    ERR << "invalid argument [--cpu=" << cpu << "]." << endl;
                    cout << helpmsg << endl;
                    exit(ERRCODE);
                }

                m_cpu.insert(m_cpu.end(), cpuno.begin(), cpuno.end());
                sort(m_cpu.begin(), m_cpu.end());
            }

            if (m_plan.empty()) {
                ERR << "invalid plan num[" << m_plan.size() << "], provide one plan at least" << "." << endl;
                cout << helpmsg << endl;
                exit(ERRCODE);
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
    ofstream outfile, logfile;
    ostream *output, *log;

    args.parse(argc, argv);

    // Get output stream.
    if (!args.m_output.empty()) {
        outfile = ofstream(args.m_output.c_str(), std::ofstream::app);
        if (!outfile.is_open()) {
            ERR << "failed to open output[" << args.m_output.c_str() << "]." << endl;
            exit(ERRCODE);
        }
        output = &outfile;
    } else {
        output = &cout;
    }

    // Get log stream.
    if (!args.m_log.empty()) {
        logfile = ofstream(args.m_log.c_str(), std::ofstream::app);
        if (!logfile.is_open()) {
            ERR << "failed to open log[" << args.m_log.c_str() << "]." << endl;
            exit(ERRCODE);
        }
        log = &logfile;
    } else {
        log = &cerr;
    }

    // CPU set has already been parsed.
    // Build profiler.
    ParallelProfiler profiler(*log);
    profiler.setCPUSet(args.m_cpu);

    // Parse config(if exists) and add plan.
    {
        ConfigParser parser;
        unordered_set<string> plans;
        Plan plan = PlanFactory::defaultPlan();
        if (!args.m_config.empty() && ConfigParser::PARSE_OK != parser.parseFile(args.m_config)) {
            ERR << "failed to parse config[" << args.m_config << "]." << endl;
            exit(ERRCODE);
        }
        for (const string& planStr : args.m_plan) {
            if (!planStr.empty() && planStr[0] == '{') {
                // Parse json plan.
                auto [_plan, error] = parser.parseJsonPlan(planStr);
                if (ConfigParser::PARSE_OK != error) {
                    ERR << "failed to add plan[" << planStr << "]." << endl;
                    exit(ERRCODE);
                } else if (!_plan.valid()) {
                    ERR << "invalid plan[" << planStr << "]." << endl;
                    exit(ERRCODE);
                }
                plan = _plan;
            } else {
                // Parse normal plan with plan id.
                auto itr = parser.getPlan(planStr);
                if (itr == parser.planEnd()) {
                    ERR << "failed to add plan[" << planStr << "]." << endl;
                    exit(ERRCODE);
                }
                plan = itr->second;
            }

            // Add plan.
            string planID = plan.getID();
            if (plans.count(planID)) {
                ERR << "conflict plan[" << planID << "] when parse argument[" << planStr << "]." << endl;
                exit(ERRCODE);
            }
            plans.insert(planID);
            profiler.addPlan(plan);
        }
    }

    // Log helper info before profile.
    {
        INFO << "Start parallel profiling with setting:" << endl;
        INFO << "[Config] " << args.m_config << endl;
        INFO << "[Output] " << (args.m_output.empty() ? "stdout" : args.m_output) << endl;
        INFO << "[Log] " << (args.m_log.empty() ? "stderr" : args.m_log) << endl;
        INFO << "[CPUSet] " << profiler.showCPUSet() << endl;
        INFO << "[Plan] " << profiler.showPlan() << endl;
    }
    *log << "[" << profiler.showPlan() << "]" << endl;

    // Do profile.
    int err = profiler.profile();
    INFO << "profile done with err[" << err << "]." << endl;

    // Do output.
    {
        if (!err && ParallelProfiler::DONE == profiler.getStatus()) {
            const auto& result = profiler.getLastResult();
            
            *output << '[' << profiler.showPlan() << ']' << endl;
            for (auto& [planid, sample] : result) {
                *output << left << setw(15) << planid;
                for (auto& [event, count] : sample) {
                    *output << "\t\t" << event << ':' << count;
                }
                *output << endl;
            }
        }
    }

    // Flush output & log stream.
    output->flush();
    log->flush();

    return err;
}
