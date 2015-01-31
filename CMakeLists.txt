cmake_minimum_required(VERSION 2.8)

project(qnxcomm)

set(CMAKE_VERBOSE_MAKEFILE true)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++0x -pthread")

add_subdirectory(userspace)
add_subdirectory(tests)

add_custom_target(kernel_module ALL COMMAND ${CMAKE_MAKE_PROGRAM}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/kernel
                  COMMENT "Linux kernel module")
