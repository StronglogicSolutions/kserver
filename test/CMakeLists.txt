cmake_minimum_required(VERSION 3.6)
project(kserver_test)
set(CMAKE_CXX_STANDARD 17)
set(KSERVER_LIB "../libkserver_lib.a")
file(GLOB_RECURSE TEST_SOURCES true src/*.cpp src/*.h*)
add_executable(${PROJECT_NAME} ${KSERVER_LIB} ${TEST_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
   "../include"
   "."
)
add_subdirectory(googletest)
target_link_libraries(${PROJECT_NAME} PUBLIC gtest)

