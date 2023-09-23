# add source file
set(SRCS
profile.cpp
ConfigParser.cpp
ResultParser.cpp
)

# add include path
include_directories(Profiler ThirdParty Utils)

# build executable
add_executable(profile ${SRCS})
target_link_libraries(profile ParallelProfiler ${Boost_LIBRARIES})

# add sub dir
add_subdirectory(Profiler)
add_subdirectory(Utils)
