# CMake version
cmake_minimum_required(VERSION 3.6)

# Compiler version
set(CMAKE_CXX_STANDARD 20)

# Project name
project(kserver)
# FPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Conditionals
if ($ENV{BUILD_ENV} STREQUAL "CIRCLECI")
  set(PQXX /usr/include/pqxx)
  set(PQXX_STATIC_LIB "") # Not needed
else ()
  set(PQXX /usr/local/include/pqxx)
  set(PQXX_STATIC_LIB "/usr/local/lib/libpqxx.a")
endif()

# External dependencies
find_package(fmt CONFIG REQUIRED)
set(LISTENERLIB "${CMAKE_SOURCE_DIR}/src/socket_listener.cpp" "${CMAKE_SOURCE_DIR}/src/task_queue.cpp")
set(KLOGGER "${CMAKE_SOURCE_DIR}/third_party/klogger/build/libklog.a")
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(JWT_INCLUDE "${CMAKE_SOURCE_DIR}/third_party/jwt-cpp/include")
find_library(UUID uuid)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)
find_package(OpenSSL COMPONENTS Crypto SSL REQUIRED)
find_package(Threads REQUIRED)

set(KSERVER_HEADERS "-I include -I third_party -I ${JWT_INCLUDE}")

# Flags
if ($ENV{BUILD_ENV} STREQUAL "CIRCLECI")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-deprecated-declarations -std=c++17 -g -lpqxx -lpq -lpthread -pthread -luuid -lstdc++fs ${KSERVER_HEADERS} -L /usr/include/pqxx")
else ()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-deprecated-declarations -std=c++17 -g -lpqxx -lpq -lpthread -pthread -luuid ${KSERVER_HEADERS} -L /usr/local/include/pqxx")
endif()

set(SOURCE_FILES
  "src/main.cpp"
  "src/database_connection.cpp"
  "include/server/kserver.cpp"
  "include/server/event_handler.cpp"
  "include/server/session.cpp"
  "include/request/controller.cpp"
  "include/config/config_parser.cpp"
  "include/system/process/scheduler.cpp"
  "include/system/process/helper.cpp"
  "include/system/process/platform.cpp"
  "include/system/process/platform_utils.cpp"
  "include/system/process/trigger.cpp"
  "include/system/process/result_parser.cpp"
  "include/system/process/research_manager.cpp"
  "include/system/process/ipc/manager.cpp"
  "include/system/process/ipc/client.cpp"
  "include/system/process/ipc/worker.cpp"
  "include/system/process/executor/executor.cpp"
  "include/system/process/executor/task_handlers/task.cpp"
  "include/system/process/executor/task_handlers/generic.cpp"
  "include/system/process/executor/task_handlers/instagram.cpp"
  "include/common/util.cpp"
  "include/common/time.cpp"
)

# Sources
if ($ENV{BUILD_ENV} STREQUAL "CIRCLECI")
  set(SOURCES ${SOURCE_FILES} ${LISTENERLIB})
else ()
  set(SOURCES ${SOURCE_FILES} ${LISTENERLIB} ${PQXX_STATIC_LIB})
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
add_library(${PROJECT_NAME}_lib STATIC ${SOURCES})

# Linking
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
  ${UUID}
  ${PQXX_LIB}
  ${PQ_LIB}
  Threads::Threads
  ${OPENSSL_LIBRARIES}
  zmq ${CMAKE_THREAD_LIBS_INIT}
  PUBLIC fmt::fmt
  ${KLOGGER}
)

target_link_libraries(
  ${PROJECT_NAME}_lib
  PRIVATE
  ${UUID}
  ${PQXX_LIB}
  ${PQ_LIB}
  Threads::Threads
  ${OPENSSL_LIBRARIES}
  zmq ${CMAKE_THREAD_LIBS_INIT}
  PUBLIC fmt::fmt
)

set(THIRD_PARTY_INCLUDES
  "third_party"
  "third_party/klogger/src"
  "third_party/kproto/include"
  "third_party/process_executor"
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "include"
  ${THIRD_PARTY_INCLUDES}
  ${JWT_INCLUDE}
)

target_include_directories(${PROJECT_NAME}_lib PRIVATE
  "include"
  ${THIRD_PARTY_INCLUDES}
  ${JWT_INCLUDE}
)


add_subdirectory(test)
