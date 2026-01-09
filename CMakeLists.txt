cmake_minimum_required(VERSION 3.10)

project(CppOverdub LANGUAGES CXX)
include_directories(3rd_party)

add_compile_options(-W -Wall)

set(CMAKE_CXX_STANDARD 23)

option(BUILD_CPPOVERDUB_TEST "Build the tests" ON)

if(BUILD_CPPOVERDUB_TEST)
add_subdirectory(Test)
endif()
