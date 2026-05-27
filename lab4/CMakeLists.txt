cmake_minimum_required(VERSION 3.16)
project(Lab4_IPC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(Receiver Receiver/Receiver.cpp)
target_include_directories(Receiver PRIVATE ${COMMON_INCLUDE})

add_executable(Sender Sender/Sender.cpp)
target_include_directories(Sender PRIVATE ${COMMON_INCLUDE})

set_target_properties(Receiver Sender PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    foreach(target Receiver Sender)
        set_target_properties(${target} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endforeach()
endif()
