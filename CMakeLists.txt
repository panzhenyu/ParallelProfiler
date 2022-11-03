cmake_minimum_required(VERSION 3.10)

project(Profiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

# set g++ flag
set(CMAKE_CXX_FLAGS
-O3
)

# add boost lib
find_package(Boost REQUIRED COMPONENTS program_options)

# add sub dirs
add_subdirectory(src)
add_subdirectory(test)
