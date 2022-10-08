
# build executable
add_executable(ProcessTest ProcessTest.cpp)
add_executable(ParallelProfilerTest ParallelProfilerTest.cpp)

# add include path
include_directories(../src)

# add shared library
target_link_libraries(ProcessTest ParallelProfiler)
target_link_libraries(ParallelProfilerTest ParallelProfiler)

