cmake_minimum_required(VERSION 3.7)

project(roce_test)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -g2 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")

add_subdirectory(common)

if(DEFINED ENV{SDE})
    message("SDE found, build switchd")
    add_subdirectory(switchd)
else()
    message("SDE not found, skip building switchd")
    add_subdirectory(rdma)
    add_subdirectory(master)
    add_subdirectory(endpoint)
endif()
