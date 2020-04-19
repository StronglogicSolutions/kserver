cmake_minimum_required(VERSION 3.6)
project(kserver_test)
set(CMAKE_CXX_STANDARD 17)
set(LISTENERLIB "../include/server/libsocket_listener.a")
set(KSERVER_LIB "libkserver_lib.a")
set(KSERVER_SRC "../src")
set(PQXX /usr/local/include/pqxx)
find_library(UUID uuid)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)
find_library(UUID uuid)
find_package(Threads REQUIRED)

file(GLOB_RECURSE TEST_SOURCES true src/*.cpp src/*.h* ../include/database/DatabaseConnection.cpp ../src/socket_listener.cpp ../src/task_queue.cpp ${KSERVER_SRC})
set(FULL_SOURCES ${TEST_SOURCES})
add_executable(${PROJECT_NAME} ${FULL_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
   "../include"
   "."
)
add_subdirectory(googletest)
target_link_libraries(${PROJECT_NAME} PUBLIC gtest ${UUID} ${PQXX_LIB} ${PQ_LIB} Threads::Threads)
