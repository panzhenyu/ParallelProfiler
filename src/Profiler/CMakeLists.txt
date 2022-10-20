# add source file
set(SRCS
PerfProfiler.cpp
ParallelProfiler.cpp
Config.cpp
ConfigFactory.cpp
)

# build library
add_library(ParallelProfiler SHARED ${SRCS})
target_link_libraries(ParallelProfiler Utils pthread pfm)
