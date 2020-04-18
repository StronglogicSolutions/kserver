 cmake_minimum_required(VERSION 3.6)
project(kserver_test)
set(CMAKE_CXX_STANDARD 17)
set(KSERVER_LIB "../libkserver_lib.a")
add_executable(${PROJECT_NAME} test.cpp run_tests.cpp ${KSERVER_LIB})
target_include_directories(${PROJECT_NAME} PUBLIC
   "../include"
   "."
   #  "googletest"
)
#target_link_libraries(${PROJECT_NAME}
#add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
add_subdirectory(googletest)
target_link_libraries(${PROJECT_NAME} PUBLIC gtest)
#target_link_libraries(${PROJECT_NAME} gtest)
