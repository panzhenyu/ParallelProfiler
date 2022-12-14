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
        CONF_FORMAT_ERROR,
        TASK_FORMAT_ERROR,
        TASK_ALREADY_EXIST,
        TASK_NOT_FOUND,
        TASK_APPEND_ERROR,
        PLAN_FORMAT_ERROR,
        PLAN_ALREADY_EXIST,
        PLAN_APPEND_ERROR,
        PLAN_INVALID,
        TASKATTR_FORMAT_ERROR,
        PERFATTR_FORMAT_ERROR,
    };

public:
    pair<Plan, ParseError> parseJsonPlan(const string& json);
    ParseError parseJson(const string& json);
    ParseError parseFile(const string& file);
    ParseError parseFile(ifstream& ifs);

    const string& getJson() const;
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
    string                      m_json;
    unordered_map<string, Task> m_taskMap;
    unordered_map<string, Plan> m_planMap;
};

//----------------------------------------------------------------------------//
// ConfigParser

inline const string&
ConfigParser::getJson() const {
    return m_json;
}
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
