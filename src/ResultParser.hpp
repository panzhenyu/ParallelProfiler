#include <vector>
#include <string>
#include <fstream>
#include <ostream>
#include "rapidjson/document.h"
#include "ParallelProfiler.hpp"

using namespace std;

class ResultParser {
public:
    ResultParser() { m_doc.Parse("[]"); }
    ~ResultParser() = default;

public:
    bool parseJson(const string& json);
    bool parseFile(const string& file);
    bool parseFile(ifstream& ifs);
    bool append(const ParallelProfiler::result_t&);

    /**
     * Result format:
     * [
     *      {
     *          "plan1": {
     *              "event": [ samples ]
     *          },
     *          "plan2": {
     *              "event": [ samples ]
     *          }
     *      }
     * ]
     */
    string json();

private:
    rapidjson::Document m_doc;
};
