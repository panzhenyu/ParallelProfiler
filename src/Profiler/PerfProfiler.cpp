
#include <boost/make_shared.hpp>
#include "PerfProfiler.hpp"
#include "Logger.hpp"

using Utils::Perf::PerfEventEncode;
using Utils::Perf::ChildEvent;

//----------------------------------------------------------------------------//
// PerfProfiler

PerfProfiler::PerfProfiler(std::ostream& log) : m_log(log) {}

/**
 * @brief Callback for Event::ProcessEvents to collect samples.
 * @param[in] e         pointer to an Event object
 * @param[in] v         used for storing samples, whose type is std::vector<sample_t>*
 * @param[in] status    type of record, a non-zero value means the type isn't PERF_RECORD_SAMPLE
 */
void
PerfProfiler::collectSample(Utils::Perf::Event* e, void* v, int status) {
    using sample_t = PerfProfiler::sample_t;

    bool hasID;
    sample_t sample;
    uint64_t readfmt, nr, i;
    std::vector<sample_t>* samples = static_cast<std::vector<sample_t>*>(v);

    if (status) { return; }
    if (nullptr == e || nullptr == v) { return; }

    readfmt = e->GetReadFormat();
    nr = e->Read<uint64_t>();

    // skip PERF_FORMAT_TOTAL_TIME_ENABLED
    if (readfmt & PERF_FORMAT_TOTAL_TIME_ENABLED) { e->Read<uint64_t>(); }

    // skip PERF_FORMAT_TOTAL_TIME_RUNNING
    if (readfmt & PERF_FORMAT_TOTAL_TIME_RUNNING) { e->Read<uint64_t>(); }

    // collect child events
    hasID = (PERF_FORMAT_ID & readfmt) == 1;
    for (i=0; i<nr; ++i) {
        sample.emplace_back(e->Read<uint64_t>());
        // skip PERF_FORMAT_ID
        if (hasID) { e->Read<uint64_t>(); }
    }

    samples->emplace_back(sample);
}

bool
PerfProfiler::collect(EventPtr event, std::vector<sample_t>& data) {
    // Event is null, collect false.
    if (nullptr == event) { return false; }

    // Not a sampling event, collect failed.
    if (0 == event->GetSamplePeriod()) { return false; }

    /**
     * TODO: Now we haven't support other sample type(except PERF_SAMPLE_READ) yet.
     * If sample type isn't PERF_SAMPLE_READ, we cannot process it.
     */
    if (PERF_SAMPLE_READ != event->GetSampleType()) { return false; }

    // The read format must enable PERF_FORMAT_GROUP.
    if (0 == (PERF_FORMAT_GROUP & event->GetReadFormat())) { return false; }

    // Collect samples.
    event->ProcessEvents(PerfProfiler::collectSample, &data);

    return true;
}

bool
PerfProfiler::collect(EventPtr event, sample_t& data) {
    // Event is null, collect false.
    if (nullptr == event) { return false; }

    uint64_t readfmt = event->GetReadFormat();
    bool hasID = readfmt & PERF_FORMAT_ID;

    // The read format must enable PERF_FORMAT_GROUP.
    if (0 == (readfmt & PERF_FORMAT_GROUP)) { return false; }

    // Add for nr.
    int skip = 1;

    // Add for time_enabled.
    if (readfmt & PERF_FORMAT_TOTAL_TIME_ENABLED) { skip++; }

    // Add for time_running.
    if (readfmt & PERF_FORMAT_TOTAL_TIME_RUNNING) { skip++; }

    // Size to read.
    size_t size = (skip + (1 + (hasID ? 1 : 0)) * (event->GetChildNum() + 1));
    size_t bytes = size * sizeof(uint64_t);
    uint64_t values[size];

    memset(values, 0, bytes);
    if (bytes != read(event->GetFd(), &values, bytes)) { return false; }

    // Collect evet count.
    for(size_t i=0, idx; i<=event->GetChildNum(); i++) {
        idx = skip + (hasID ? (i<<1) : i);
        data.emplace_back(values[idx]);
    }

    return true;
}

PerfProfiler::EventPtr
PerfProfiler::initEvent(const PerfAttribute& perf) {
    EventPtr output;
    std::string curEvent;
    PerfEventEncode curEncode;
    std::vector<PerfEventEncode> encodes;
    const std::vector<std::string> events = perf.getEvents();

    // Get all event encodes.
    for (size_t i=0; i<=events.size(); ++i) {
        curEvent = i == 0 ? perf.getLeader() : events[i-1];
        if (!Utils::Perf::getPerfEventEncoding(curEvent, curEncode)) {
            LOG(ERROR) << "failed to get encoding for event[" << curEvent << "].";
            return nullptr;
        }
        encodes.emplace_back(curEncode);
    }

    // Create event.
    output = boost::make_shared<Utils::Perf::Event>(-1, encodes[0].config, encodes[0].type, perf.getPeriod(), 0);

    // Attach child events.
    for (size_t i=1; i<encodes.size(); ++i) {
        output->AttachEvent(boost::make_shared<ChildEvent>(encodes[i].config, encodes[i].type));
    }

    return output;
}

//----------------------------------------------------------------------------//
