# add source file
set(SRCS
ParallelProfile.cpp
)

# add include path
include_directories(Profiler ThirdParty Utils)

# build executable
add_executable(profiler profiler.cpp)
target_link_libraries(profiler ParallelProfiler)

# add sub dir
add_subdirectory(Profiler)
add_subdirectory(Utils)
