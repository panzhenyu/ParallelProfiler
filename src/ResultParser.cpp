#include <iostream>
#include "ResultParser.hpp"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

bool ResultParser::parseJson(const string& json) {
    // TODO: add format check
    return !m_doc.Parse(json.c_str()).HasParseError() && m_doc.IsArray();
}

bool ResultParser::parseFile(const string& file) {
    ifstream ifs(file, ios_base::in);
    return parseFile(ifs);
}

bool ResultParser::parseFile(ifstream& ifs) {
    string json;
    size_t size;

    if (!ifs.is_open()) {
        return false;
    }

    json.resize(ifs.seekg(0, ios_base::end).tellg());
    if (ifs.fail()) {
        return false;
    }

    if (ifs.seekg(0, ios_base::beg).read(json.data(), json.size()).fail()) {
        return false;
    }

    return parseJson(json);
}

bool ResultParser::append(const ParallelProfiler::result_t& result) {
    if (!m_doc.IsArray()) {
        cout << "[ERROR] doc isn't an array." << endl;
        return false;
    }

    rapidjson::Document::AllocatorType& allocator = m_doc.GetAllocator();
    rapidjson::Value resultObject(rapidjson::Type::kObjectType);

    for (auto& [plan, samples] : result) {
        // for each plan object
        auto& leader = plan.getPerfAttribute().getLeader();
        auto& events = plan.getPerfAttribute().getEvents();
        rapidjson::Value planObject(rapidjson::Type::kObjectType);

        // for each sample
        for (auto& sample : samples) {
            // check whether sample matches leader & events;
            if (sample.size() != events.size()+1) {
                cout << "[ERROR] sample size[" << sample.size() << "] miss match events[" << events.size()+1 << "]" << endl;
                return false;
            }

            // for each event
            for (int i=0; i<sample.size(); ++i) {
                auto& event = i == 0 ? leader : events[i-1];
                if (!planObject.HasMember(event.c_str())) {
                    planObject.AddMember(rapidjson::Value(event.c_str(), allocator).Move(), 
                        rapidjson::Value(rapidjson::Type::kArrayType).Move(), allocator);
                }
                planObject[event.c_str()].PushBack(sample[i], allocator);
            }
        }
        resultObject.AddMember(rapidjson::Value(plan.getID().c_str(), allocator).Move(), move(planObject), allocator);
    }
    m_doc.PushBack(move(resultObject), allocator);
    return true;
}

string ResultParser::json() {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    m_doc.Accept(writer);
    return buffer.GetString();
}
