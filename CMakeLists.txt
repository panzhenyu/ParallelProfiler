cmake_minimum_required(VERSION 3.10)

project(Profiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_CXX_FLAGS
-O3
)

add_subdirectory(src)
add_subdirectory(test)
