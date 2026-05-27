cmake_minimum_required(VERSION 3.16)

project(OS_Labs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_subdirectory(lab1)
    add_subdirectory(lab2)
    add_subdirectory(lab3)
    add_subdirectory(lab4)
    add_subdirectory(lab5)
else()
    message(WARNING "Labs 1-5 use WinAPI and are built only on Windows.")
endif()

add_subdirectory(lab6)
