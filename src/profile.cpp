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
#include "ResultParser.hpp"
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
        --plan          Repeated        plan id, at least one plan                                              \n\
        --json-plan     Repeated        such \"{key:value[,key:value]}\", at least one plan                     \n\
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
    void parseCPU(string& cpu) {
        int pos, begin, end;
        unordered_set<int> cpuno;
        vector<string> cpusetSplit;

        if (cpu.empty()) { return; }

        // Case: --cpu=1,2~4
        boost::split(cpusetSplit, cpu, boost::is_any_of(","), boost::token_compress_on);
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

        m_cpu.insert(m_cpu.end(), cpuno.begin(), cpuno.end());
        sort(m_cpu.begin(), m_cpu.end());
    }

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
            ("json-plan", po::value<vector<string>>(&m_jsonPlan)->multitoken(), "plan in json format")
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
            try {
                parseCPU(cpu);
            } catch (...) {
                ERR << "invalid argument [--cpu=" << cpu << "]." << endl;
                cout << helpmsg << endl;
                exit(ERRCODE);
            }

            if (m_plan.empty() && m_jsonPlan.empty()) {
                ERR << "provide one plan at least" << "." << endl;
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
    vector<string>      m_jsonPlan;
};

int main(int argc, char *argv[]) {
    ProfilerArguments args;
    ofstream logfile;
    ostream *log;

    args.parse(argc, argv);

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

    // Add cpu set.
    profiler.setCPUSet(args.m_cpu);

    // Parse config(if exists) and add plan.
    {
        ConfigParser parser;
        unordered_set<string> exist;
        ConfigParser::ParseError error;

        // Parse config file.
        if (!args.m_config.empty() && ConfigParser::PARSE_OK != (error=parser.parseFile(args.m_config))) {
            ERR << "failed to parse config[" << args.m_config << "] with errcode[" << error << "]." << endl;
            exit(ERRCODE);
        }

        // Parse plan id.
        for (const string& planid : args.m_plan) {
            auto itr = parser.getPlan(planid);
            if (itr == parser.planEnd()) {
                ERR << "failed to add plan[" << planid << "]." << endl;
                exit(ERRCODE);
            }
            if (!exist.count(planid)) {
                profiler.addPlan(itr->second);
                exist.insert(planid);
            } else {
                ERR << "conflict plan[" << planid << "] when parse argument[" << planid << "]." << endl;
                exit(ERRCODE);
            }
        }

        // Parse json plan.
        for (const string& jsonPlan : args.m_jsonPlan) {
            auto [plan, error] = parser.parseJsonPlan(jsonPlan);
            if (ConfigParser::PARSE_OK != error) {
                ERR << "failed to add plan[" << jsonPlan << "]." << endl;
                exit(ERRCODE);
            } else if (!plan.valid()) {
                ERR << "invalid plan[" << jsonPlan << "]." << endl;
                exit(ERRCODE);
            }
            if (!exist.count(plan.getID())) {
                profiler.addPlan(plan);
                exist.insert(plan.getID());
            } else {
                ERR << "conflict plan[" << plan.getID() << "] when parse argument[" << plan.getID() << "]." << endl;
                exit(ERRCODE);
            }
        }
    }

    // Do profile.
    *log << "[" << profiler.showPlan() << "]" << endl;
    int err = profiler.profile();
    INFO << "profile done with err[" << err << "]." << endl;

    // Do output.
    if (!err && ParallelProfiler::DONE == profiler.getStatus()) {
        ResultParser result;

        if (args.m_output.empty()) {
            // Output to cout.
            result.append(profiler.getLastResult());
            cout << result.json() << endl;
        } else {
            // Append to file.
            result.parseFile(args.m_output);

            ofstream outfile(args.m_output.c_str(), std::ofstream::out);
            if (!outfile.is_open()) {
                ERR << "failed to open output[" << args.m_output.c_str() << "]." << endl;
                exit(ERRCODE);
            }
            if (!result.append(profiler.getLastResult())) {
                ERR << "failed to append result." << endl;
                exit(ERRCODE);
            }
            outfile << result.json();
            outfile.close();
        }
    }

    // Flush log stream.
    log->flush();

    return err;
}
