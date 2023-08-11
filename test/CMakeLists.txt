cmake_minimum_required(VERSION 3.6)
project(kserver_test)
set(CMAKE_CXX_STANDARD 20)

# FPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(LISTENERLIB "../include/server/libsocket_listener.a")
set(KSERVER_LIB "libkserver_lib.a")
set(KSERVER_SRC "../src")
set(KLOGGER "${CMAKE_SOURCE_DIR}/third_party/klogger/build/libklog.a")
set(MOCK_DATA_SRC "src/server/client/mock_data.cpp")
set(PQXX /usr/local/include/pqxx)
find_library(UUID uuid)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)
find_library(UUID uuid)
find_package(OpenSSL COMPONENTS Crypto SSL REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt CONFIG REQUIRED)
set(JWT_INCLUDE "${CMAKE_SOURCE_DIR}/third_party/jwt-cpp/include")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -I ${JWT_INCLUDE}")
file(GLOB_RECURSE TEST_SOURCES true ../include/system/process/executor/task_handlers/task.cpp ../include/*.cpp src/*.cpp src/*.h* ../src/database_connection.cpp ../src/socket_listener.cpp ../src/task_queue.cpp ${MOCK_DATA_SRC} ../third_party/klogger/src/logger.cpp)

set(FULL_SOURCES ${TEST_SOURCES})

add_executable(${PROJECT_NAME} ${FULL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
   "."
   "../include"
   "../third_party"
   "../third_party/klogger/src"
   "../third_party/kproto/include"
   ${JWT_INCLUDE}
)

add_subdirectory(googletest)

target_link_libraries(
   ${PROJECT_NAME} PRIVATE
   gtest
   gmock
   ${UUID}
   ${PQXX_LIB}
   ${PQ_LIB}
   Threads::Threads
   ${OPENSSL_LIBRARIES}
   zmq ${CMAKE_THREAD_LIBS_INIT}
   PUBLIC fmt::fmt
   ${KLOGGER}
)
