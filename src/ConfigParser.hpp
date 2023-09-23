#pragma once

#include <string>
#include <iosfwd>
#include <type_traits>
#include <unordered_map>
#include "Profiler/Config.hpp"
#include "rapidjson/document.h"
using namespace std;

/**
 * ConfigParser
 */
class ConfigParser {
public:
    enum ParseError {
        PARSE_OK,
        FILE_NOT_EXIST,
        FILE_SEEK_ERROR,

        CONF_PARSE_ERROR,
        CONF_INVALID_FORMAT,
        TASK_INVALID_FORMAT,
        TASK_ALREADY_EXIST,
        TASK_APPEND_ERROR,
        PLAN_INVALID_FORMAT,
        PLAN_INVALID,
        PLAN_ALREADY_EXIST,
        PLAN_APPEND_ERROR,

        TASK_LOST_MEMBER,
        TASK_INVALID_MEMBER,
        TASK_INVALID_DIR,

        PLAN_LOST_MEMBER,
        PLAN_INVALID_MEMBER,

        TASKATTR_INVALID_FORMAT,
        TASKATTR_INVALID_ID,
        TASKATTR_INVALID_TASK,
        TASKATTR_INVALID_PARAM,
        TASKATTR_INVALID_PARAMARG,
        TASKATTR_INVALID_RT,
        TASKATTR_INVALID_PINCPU,
        TASKATTR_INVALID_PHASE,

        PERFATTR_INVALID_FORMAT,
        PERFATTR_INVALID_LEADER,
        PERFATTR_INVALID_PERIOD,
        PERFATTR_INVALID_MEMBER,
        PERFATTR_INVALID_EVENT,
    };

public:
    pair<Plan, ParseError> parseJsonPlan(const string& json);
    ParseError parseJson(const string& json);
    ParseError parseFile(const string& file);
    ParseError parseFile(ifstream& ifs);

    unordered_map<string, Task>::const_iterator taskBegin() const;
    unordered_map<string, Task>::const_iterator taskEnd() const;
    unordered_map<string, Task>::const_iterator getTask(const string& taskID) const;
    unordered_map<string, Plan>::const_iterator planBegin() const;
    unordered_map<string, Plan>::const_iterator planEnd() const;
    unordered_map<string, Plan>::const_iterator getPlan(const string& planID) const;

private:
    pair<Task, ParseError> parseTask(const rapidjson::Value& val);
    pair<Plan, ParseError> parsePlan(const rapidjson::Value& val);
    pair<TaskAttribute, ParseError> parseTaskAttribute(const rapidjson::Value& val);
    pair<PerfAttribute, ParseError> parsePerfAttribute(const rapidjson::Value& val);

private:
    unordered_map<string, Task> m_taskMap;
    unordered_map<string, Plan> m_planMap;
};

//----------------------------------------------------------------------------//
// ConfigParser

inline unordered_map<string, Task>::const_iterator
ConfigParser::taskBegin() const {
    return m_taskMap.begin();
}

inline unordered_map<string, Task>::const_iterator
ConfigParser::taskEnd() const {
    return m_taskMap.end();
}

inline unordered_map<string, Task>::const_iterator
ConfigParser::getTask(const string& taskID) const {
    return m_taskMap.find(taskID);
}

inline unordered_map<string, Plan>::const_iterator
ConfigParser::planBegin() const {
    return m_planMap.begin();
}

inline unordered_map<string, Plan>::const_iterator
ConfigParser::planEnd() const {
    return m_planMap.end();
}

inline unordered_map<string, Plan>::const_iterator
ConfigParser::getPlan(const string& planID) const {
    return m_planMap.find(planID);
}

//----------------------------------------------------------------------------//
