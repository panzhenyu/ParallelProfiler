#pragma once

#include <string>
#include <iosfwd>
#include <type_traits>
#include <unordered_map>
#include "Profiler/Config.hpp"
using namespace std;

/**
 * Config
 * Parse plan config file, provide plan object
 */
class Config {
public:
    bool parseJson(const string& json);
    bool parseFile(const string& file);
    bool parseFile(ifstream& ifs);
    const string& getJson() const;
    unordered_map<string, Task>::const_iterator getTask(const string& taskID) const;
    unordered_map<string, Plan>::const_iterator getPlan(const string& planID) const;

private:
    template <typename _Tp, typename enable_if<is_same<remove_const_t<remove_reference_t<_Tp>>, Task>::value, Task>::type* = nullptr>
    bool insertTask(_Tp&& task) {
        if (m_taskMap.count(task.getID())) {
            return false;
        }
        return m_taskMap.emplace(forward<_Tp>(task)).second;
    }

    template <typename _Tp, typename enable_if<is_same<remove_const_t<remove_reference_t<_Tp>>, Plan>::value, Plan>::type* = nullptr>
    bool insertPlan(_Tp&& plan) {
        if (m_planMap.count(plan.getID())) {
            return false;
        }
        return m_planMap.emplace(forward<_Tp>(plan)).second;
    }

private:
    string                      m_json;
    unordered_map<string, Task> m_taskMap;
    unordered_map<string, Plan> m_planMap;
};

//----------------------------------------------------------------------------//
// Config


inline const string&
Config::getJson() const {
    return m_json;
}

inline unordered_map<string, Task>::const_iterator
Config::getTask(const string& taskID) const {
    return m_taskMap.find(taskID);
}

inline unordered_map<string, Plan>::const_iterator
Config::getPlan(const string& planID) const {
    return m_planMap.find(planID);
}

//----------------------------------------------------------------------------//
