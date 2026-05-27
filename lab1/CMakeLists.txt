cmake_minimum_required(VERSION 3.16)
project(Lab1_ProcessCreation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common include directory (shared headers: employee.h, win_error.h)
set(COMMON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---- Creator ----------------------------------------------------------------
add_executable(Creator
    Creator/Creator.cpp
)
target_include_directories(Creator PRIVATE ${COMMON_INCLUDE_DIR})

# ---- Reporter ---------------------------------------------------------------
add_executable(Reporter
    Reporter/Reporter.cpp
)
target_include_directories(Reporter PRIVATE ${COMMON_INCLUDE_DIR})

# ---- Main -------------------------------------------------------------------
add_executable(Main
    Main/Main.cpp
)
target_include_directories(Main PRIVATE ${COMMON_INCLUDE_DIR})

# All three executables must be in the same directory so Main can find them
set_target_properties(Creator Reporter Main
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Windows subsystem: console for all three
if(MSVC)
    foreach(target Creator Reporter Main)
        set_target_properties(${target} PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endforeach()
endif()
