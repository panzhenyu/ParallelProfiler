#include <fstream>
#include <iostream>
#include "ConfigParser.hpp"
#include "ConfigFactory.hpp"

static unordered_map<string, Plan::Type> planType = {
    { "DAEMON",         Plan::Type::DAEMON },
    { "COUNT",          Plan::Type::COUNT },
    { "SAMPLE_ALL",     Plan::Type::SAMPLE_ALL },
    { "SAMPLE_PHASE",   Plan::Type::SAMPLE_PHASE },
};

ConfigParser::ParseError
ConfigParser::parseJson(const string& json) {
    rapidjson::Document doc;

    if (doc.Parse(json.c_str()).HasParseError()) {
        return CONF_FORMAT_ERROR;
    }

    if (!doc.IsObject()) {
        return CONF_FORMAT_ERROR;
    }

    m_json = json;
    m_taskMap.clear();
    m_planMap.clear();

    // Parse Task if exist.
    if (doc.HasMember("Task")) {
        const auto& tasks = doc["Task"];
        if (!tasks.IsArray()) {
            return TASK_FORMAT_ERROR;
        }

        for (auto cur=tasks.Begin(); cur<tasks.End(); ++cur) {
            auto [task, err] = parseTask(*cur);
            if (PARSE_OK != err) {
                cout << "[ERROR] parse task failed at index[" << cur-tasks.Begin() << "]." << endl;
                return err;
            }

            // Parse task succeed, try to add it.
            if (m_taskMap.count(task.getID())) {
                cout << "[ERROR] task[" << task.getID() << "] already exists." << endl;
                return TASK_ALREADY_EXIST;
            }
            if (!m_taskMap.emplace(task.getID(), std::move(task)).second) {
                return TASK_APPEND_ERROR;
            }
        }
    }

    // Parse Plan if exist.
    if (doc.HasMember("Plan")) {
        const auto& plans = doc["Plan"];
        if (!plans.IsArray()) {
            return PLAN_FORMAT_ERROR;
        }

        for (auto cur=plans.Begin(); cur<plans.End(); ++cur) {
            auto [plan, err] = parsePlan(*cur);
            if (PARSE_OK != err) {
                cout << "[ERROR] parse plan failed at index[" << cur-plans.Begin() << "]." << endl;
                return err;
            }

            // Parse plan succeed, try to add it.
            if (!plan.valid()) {
                cout << "[ERROR] plan[" << plan.getID() << "] is invalid." << endl;
                return PLAN_INVALID;
            }
            if (m_planMap.count(plan.getID())) {
                cout << "[ERROR] plan[" << plan.getID() << "] already exists." << endl;
                return PLAN_ALREADY_EXIST;
            }
            if (!m_planMap.emplace(plan.getID(), std::move(plan)).second) {
                return PLAN_APPEND_ERROR;
            }
        }
    }

    return PARSE_OK;
}

pair<Task, ConfigParser::ParseError>
ConfigParser::parseTask(const rapidjson::Value& val) {
    Task task = TaskFactory::defaultTask();

    // Parse each task.
    if (!val.IsObject() || !val.HasMember("id") || !val.HasMember("cmd")) {
        return {task, TASK_FORMAT_ERROR};
    }

    // Check id & cmd.
    const auto& id = val["id"];
    const auto& cmd = val["cmd"];
    if (!id.IsString() || !cmd.IsString()) {
        return {task, TASK_FORMAT_ERROR};
    }
    task.setID(id.GetString());
    task.setCmd(cmd.GetString());

    // Check dir, if exists.
    if (val.HasMember("dir")) {
        const auto& dir = val["dir"];
        if (dir.IsString()) {
            task.setDir(dir.GetString());
        } else {
            return {task, TASK_FORMAT_ERROR};
        }
    } else { task.setDir("."); }

    return {task, PARSE_OK};
}

pair<Plan, ConfigParser::ParseError>
ConfigParser::parsePlan(const rapidjson::Value& val) {
    Plan plan = PlanFactory::defaultPlan("planid", Plan::Type::DAEMON);

    // Parse each plan.
    if (!val.IsObject() || !val.HasMember("id") || !val.HasMember("type")) {
        return {plan, PLAN_FORMAT_ERROR};
    }

    // Check id & type.
    const auto& id = val["id"];
    const auto& type = val["type"];
    if (!id.IsString() || !type.IsString() || !planType.count(type.GetString())) {
        cout << "2" << endl;
        return {plan, PLAN_FORMAT_ERROR};
    }
    plan.setID(id.GetString());
    plan.setType(planType[type.GetString()]);

    // Parse TaskAttribute if exists.
    if (val.HasMember("task")) {
        auto [taskAttr, err] = parseTaskAttribute(val["task"]);
        if (err != PARSE_OK) {
            return {plan, err};
        }
        plan.setTaskAttribute(std::move(taskAttr));
    }

    // Parse PerfAttritbue if exists.
    if (val.HasMember("perf")) {
        auto [perfAttr, err] = parsePerfAttribute(val["perf"]);
        if (err != PARSE_OK) {
            return {plan, err};
        }
        plan.setPerfAttribute(std::move(perfAttr));
    }

    return {plan, PARSE_OK};
}

pair<TaskAttribute, ConfigParser::ParseError>
ConfigParser::parseTaskAttribute(const rapidjson::Value& val) {
    TaskAttribute attr = TaskAttributeFactory::defaultTaskAttribute();
    vector<string> param;

    if (!val.IsObject() || !val.HasMember("id")) {
        return {attr, TASKATTR_FORMAT_ERROR};
    }

    // Parse id.
    const auto& id = val["id"];
    if (!id.IsString()) {
        return {attr, TASKATTR_FORMAT_ERROR};
    }
    if (!m_taskMap.count(id.GetString())) {
        return {attr, TASK_NOT_FOUND};
    }
    attr.setTask(m_taskMap.at(id.GetString()));

    // Parse param if exists.
    if (val.HasMember("param")) {
        const auto& paramVal = val["param"];
        if (!paramVal.IsArray()) {
            return {attr, TASKATTR_FORMAT_ERROR};
        }
        for (auto cur=paramVal.Begin(); cur<paramVal.End(); ++cur) {
            if (!cur->IsString()) {
                return {attr, TASKATTR_FORMAT_ERROR};
            }
            param.emplace_back(cur->GetString());
        }
        attr.setParam(std::move(param));
    }

    // Parse rt if exists.
    if (val.HasMember("rt")) {
        const auto& rt = val["rt"];
        if (!rt.IsBool()) {
            return {attr, TASKATTR_FORMAT_ERROR};
        }
        attr.setRT(rt.GetBool());
    }

    // Parse pincpu if exists.
    if (val.HasMember("pincpu")) {
        const auto& pincpu = val["pincpu"];
        if (!pincpu.IsBool()) {
            return {attr, TASKATTR_FORMAT_ERROR};
        }
        attr.setPinCPU(pincpu.GetBool());
    }

    // Parse phase if exists.
    if (val.HasMember("phase")) {
        const auto& phase = val["phase"];
        if (!phase.IsArray() || 2 != phase.Size() || !phase[0].IsUint64() || !phase[1].IsUint64()) {
            return {attr, TASKATTR_FORMAT_ERROR};
        }
        attr.setPhaseBegin(phase[0].GetUint64());
        attr.setPhaseEnd(phase[1].GetUint64());
    }

    return {attr, PARSE_OK};
}

pair<PerfAttribute, ConfigParser::ParseError>
ConfigParser::parsePerfAttribute(const rapidjson::Value& val) {
    PerfAttribute attr = PerfAttributeFactory::defaultPerfAttribute();
    vector<string> events;
    
    if (!val.IsObject()) {
        return {attr, PERFATTR_FORMAT_ERROR};
    }

    // Parse leader if exists.
    if (val.HasMember("leader")) {
        const auto& leader = val["leader"];
        if (!leader.IsString()) {
            return {attr, PERFATTR_FORMAT_ERROR};
        }
        attr.setLeader(leader.GetString());
    }

    // Parse period if exists.
    if (val.HasMember("period")) {
        const auto& period = val["period"];
        if (!period.IsUint64()) {
            return {attr, PERFATTR_FORMAT_ERROR};
        }
        attr.setPeriod(period.GetUint64());
    }

    // Parse member if exists.
    if (val.HasMember("member")) {
        const auto& member = val["member"];
        if (!member.IsArray()) {
            return {attr, PERFATTR_FORMAT_ERROR};
        }
        for (auto cur=member.Begin(); cur<member.End(); ++cur) {
            if (!cur->IsString()) {
                return {attr, PERFATTR_FORMAT_ERROR};
            }
            events.emplace_back(cur->GetString());
        }
        attr.setEvents(std::move(events));
    }

    return {attr, PARSE_OK};
}

ConfigParser::ParseError
ConfigParser::parseFile(const string& file) {
    ifstream ifs(file, ios_base::in);
    return parseFile(ifs);
}

ConfigParser::ParseError
ConfigParser::parseFile(ifstream& ifs) {
    string json;
    size_t size;

    if (!ifs.is_open()) {
        return FILE_NOT_EXIST;
    }

    json.resize(ifs.seekg(0, ios_base::end).tellg());
    if (ifs.fail()) {
        return FILE_SEEK_ERROR;
    }

    if (ifs.seekg(0, ios_base::beg).read(json.data(), json.size()).fail()) {
        return FILE_SEEK_ERROR;
    }

    return parseJson(json);
}
