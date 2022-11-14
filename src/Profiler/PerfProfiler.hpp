#pragma once

#include <vector>
#include <cstdint>
#include <ostream>
#include "Config.hpp"
#include "PerfEventWrapper.hpp"

struct IPerfProfiler {
public:
    using sample_t  = std::vector<uint64_t>;
    using Event     = Utils::Perf::Event;
    using EventPtr  = boost::shared_ptr<Event>;

public:
    virtual int profile() = 0;
    virtual bool collect(EventPtr event, std::vector<sample_t>& data) = 0;
    virtual bool collect(EventPtr event, sample_t& data) = 0;
    virtual EventPtr initEvent(const PerfAttribute& perf) = 0;
};

class PerfProfiler: public IPerfProfiler {
private:
    /**
     * @brief Callback for Event::ProcessEvents to collect samples.
     * @param[in] e         pointer to an Event object
     * @param[in] v         used for storing samples, whose type is std::vector<sample_t>*
     * @param[in] status    type of record, a non-zero value means the type isn't PERF_RECORD_SAMPLE
     */
    static void collectSample(Utils::Perf::Event* e, void* v, int status);

public:
    PerfProfiler(std::ostream& log);

    /**
     * @brief Profile a task, not implement yet.
     */
    virtual int profile() { return true; }

    /**
     * @brief Collect sample event by reading mmap buffer.
     */
    virtual bool collect(EventPtr event, std::vector<sample_t>& data);

    /**
     * @brief Collect sample/count event by reading event fd.
     */
    virtual bool collect(EventPtr event, sample_t& data);

    /**
     * @brief Create an event group with group leader(leader) and group member(memberEvents).
     * @param[in] perf Describe a static perf attribute.
     * @returns nullptr if failed.
     */
    virtual EventPtr initEvent(const PerfAttribute& perf);

protected:
    /**
     * @brief Ostream object for log.
     */
    std::ostream&               m_log;

};
