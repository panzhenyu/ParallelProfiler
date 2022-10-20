#pragma once

#include <vector>
#include <cstdint>
#include <ostream>
#include "Config.hpp"
#include "PerfEventWrapper.hpp"

struct IPerfProfiler {
    virtual int profile() = 0;
};

class PerfProfiler: public IPerfProfiler {
public:
    using sample_t  = std::vector<uint64_t>;
    using EventPtr  = boost::shared_ptr<Utils::Perf::Event>;

public:
    static void collectSample(Utils::Perf::Event* e, void* v, int status);

public:
    PerfProfiler(std::ostream& log, std::ostream& output);

    /**
     * @brief Profile a task, not implement yet.
     */
    virtual int profile() { return true; }

    /**
     * @brief Collect sample event by reading mmap buffer.
     */
    bool collect(EventPtr event, std::vector<sample_t>& data);

    /**
     * @brief Collect sample/count event by reading event fd.
     */
    bool collect(EventPtr event, sample_t& data);

    /**
     * @brief Create an event group with group leader(leader) and group member(memberEvents).
     * @param[in] perf Describe a static perf attribute.
     * @returns nullptr if failed.
     */
    EventPtr initEvent(const PerfAttribute& perf);

protected:
    /**
     * @brief Ostream object for log.
     */
    std::ostream&               m_log;

    /**
     * @brief Ostream object for output perf data.
     */
    std::ostream&               m_output;

};
