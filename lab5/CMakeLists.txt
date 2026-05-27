cmake_minimum_required(VERSION 3.16)
project(Lab5_NamedPipes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(Server Server/Server.cpp)
target_include_directories(Server PRIVATE ${COMMON_INCLUDE})

add_executable(Client Client/Client.cpp)
target_include_directories(Client PRIVATE ${COMMON_INCLUDE})

set_target_properties(Server Client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    foreach(target Server Client)
        set_target_properties(${target} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endforeach()
endif()
