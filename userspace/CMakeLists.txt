cmake_minimum_required(VERSION 2.8)

include_directories(. ../kernel)

add_library(qnxcomm STATIC
   userspace.cpp
)
