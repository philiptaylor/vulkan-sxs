cmake_minimum_required (VERSION 2.8)

project(vulkan-sxs)

include_directories(
    third_party
    .
)

add_executable(01-device
    01-device/main.cpp
)

add_executable(02-extensions
    02-extensions/main.cpp
    common/Common.h
    common/Log.cpp
    common/Log.h
)

add_executable(03-allocator-callbacks
    03-allocator-callbacks/main.cpp
    common/AllocationCallbacks.cpp
    common/AllocationCallbacks.h
    common/AutoWrappers.h
    common/Common.h
    common/DeviceLoader.cpp
    common/DeviceLoader.h
    common/InstanceFunctions.h
    common/Log.cpp
    common/Log.h
)

add_executable(04-clear
    04-clear/main.cpp
    common/AllocationCallbacks.cpp
    common/AllocationCallbacks.h
    common/AutoWrappers.h
    common/Common.h
    common/DeviceLoader.cpp
    common/DeviceLoader.h
    common/InstanceFunctions.h
    common/Log.cpp
    common/Log.h
)
