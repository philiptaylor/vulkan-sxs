cmake_minimum_required (VERSION 2.8)

project(vulkan-sxs)

include_directories(third_party)

add_executable(01-device 01-device/main.cpp)
