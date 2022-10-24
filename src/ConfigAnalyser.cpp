#include <fstream>
#include <iostream>
#include "ConfigAnalyser.hpp"
#include "rapidjson/document.h"

bool
Config::parseJson(const string& json) {
    rapidjson::Document doc;

    m_json = json;
    doc.Parse(m_json.c_str());

    return true;
}


bool
Config::parseFile(const string& file) {
    ifstream ifs(file, ios_base::in);
    return parseFile(ifs);
}

bool
Config::parseFile(ifstream& ifs) {
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
