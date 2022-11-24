# set cxx flags
set(CMAKE_CXX_FLAGS
-g
)

# build executable
add_executable(ProcessTest ProcessTest.cpp)
target_link_libraries(ProcessTest ParallelProfiler)

add_executable(ParallelProfilerTest ParallelProfilerTest.cpp)
target_link_libraries(ParallelProfilerTest ParallelProfiler)

add_executable(PerfWrapperTest PerfWrapperTest.cpp)
target_link_libraries(PerfWrapperTest ParallelProfiler)

add_executable(PerfSampleTest PerfSampleTest.cpp)
target_link_libraries(PerfSampleTest ParallelProfiler)

add_executable(ConfigParserTest ConfigParserTest.cpp ../src/ConfigParser.cpp)
target_link_libraries(ConfigParserTest ParallelProfiler)

add_executable(ResultParserTest ResultParserTest.cpp ../src/ResultParser.cpp)
target_link_libraries(ResultParserTest ParallelProfiler)

# add include path
include_directories(../src/Profiler ../src/ThirdParty ../src/Utils ../src)
