set(SRCS
    ParallelProfiler.cpp
    PerfEventWrapper.cpp
    Process.cpp
)

# build library
add_library(ParallelProfiler SHARED ${SRCS})
target_link_libraries(ParallelProfiler pthread pfm)

# build executable
add_executable(profiler profiler.cpp)

# add include path
include_directories(ThirdParty)

# add shared library
target_link_libraries(profiler ParallelProfiler)
