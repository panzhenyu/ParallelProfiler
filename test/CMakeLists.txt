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

add_executable(ConfigAnalyserTest ConfigAnalyserTest.cpp ../src/ConfigAnalyser.cpp)
target_link_libraries(ConfigAnalyserTest ParallelProfiler)

# add include path
include_directories(../src/Profiler ../src/ThirdParty ../src/Utils ../src)
