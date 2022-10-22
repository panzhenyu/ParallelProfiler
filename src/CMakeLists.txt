# add source file
set(SRCS
ParallelProfile.cpp
)

# add include path
include_directories(Profiler ThirdParty Utils)

# build executable
add_executable(ParallelProfile ParallelProfile.cpp)
target_link_libraries(ParallelProfile ParallelProfiler ${Boost_LIBRARIES})

# add sub dir
add_subdirectory(Profiler)
add_subdirectory(Utils)
