# add source file
set(SRCS
PerfEventWrapper.cpp
PosixUtil.cpp
)

# build library
add_library(Utils SHARED ${SRCS})
