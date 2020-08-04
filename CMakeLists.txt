# CMake version
cmake_minimum_required(VERSION 3.6)

# Compiler version
set(CMAKE_CXX_STANDARD 17)

# Project name
project(kserver)

# Conditionals
if ($ENV{BUILD_ENV} STREQUAL "CIRCLECI")
  set(PQXX /usr/include/pqxx)
  set(PQXX_STATIC_LIB "") # Not needed
else ()
  set(PQXX /usr/local/include/pqxx)
  set(PQXX_STATIC_LIB "/usr/local/lib/libpqxx.a")
endif()

# External dependencies
set(LISTENERLIB "${CMAKE_SOURCE_DIR}/include/server/libsocket_listener.a")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_library(UUID uuid)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)
find_package(Threads REQUIRED)

set(KSERVER_HEADERS -I include -I third_party)

# Flags
if ($ENV{BUILD_ENV} STREQUAL "CIRCLECI")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -g -lpqxx -lpq -lpthread -pthread -luuid -lstdc++fs ${KSERVER_HEADERS} -L /usr/include/pqxx")
else ()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -g -lpqxx -lpq -lpthread -pthread -luuid ${KSERVER_HEADERS} -L /usr/local/include/pqxx")
endif()

# Sources
if ($ENV{BUILD_ENV} STREQUAL "CIRCLECI")
  set(SOURCES "src/main.cpp" "src/database_connection.cpp" ${LISTENERLIB})
else ()
  set(SOURCES "src/main.cpp" "src/database_connection.cpp" ${LISTENERLIB} ${PQXX_STATIC_LIB})
endif()


add_executable(${PROJECT_NAME} ${SOURCES})
add_library(${PROJECT_NAME}_lib STATIC ${SOURCES})
# Linking
target_link_libraries(${PROJECT_NAME} ${LISTENERLIB} ${UUID} ${PQXX_LIB} ${PQ_LIB} Threads::Threads)
target_link_libraries(${PROJECT_NAME}_lib ${LISTENERLIB} ${UUID} ${PQXX_LIB} ${PQ_LIB} Threads::Threads)

# Includes
target_include_directories(${PROJECT_NAME} PRIVATE
    "include"
    "third_party"
 )
target_include_directories(${PROJECT_NAME}_lib PRIVATE
    "include"
    "third_party"
 )
 add_subdirectory(test)

