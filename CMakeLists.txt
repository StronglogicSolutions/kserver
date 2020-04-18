# CMake version
cmake_minimum_required(VERSION 3.6)

# Compiler version
set(CMAKE_CXX_STANDARD 17)

# Project name
project(kserver)

# External dependencies
set(LISTENERLIB "${CMAKE_SOURCE_DIR}/include/server/libsocket_listener.a")
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(PQXX /usr/local/include/pqxx)
find_library(UUID uuid)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)
find_package(Threads REQUIRED)

# Sources
set(SOURCES "src/main.cpp" "include/database/DatabaseConnection.cpp" ${LISTENERLIB} "/usr/local/lib/libpqxx.a")

#Flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -g -lpqxx -lpq -lpthread -pthread -luuid -I ./include -L /usr/local/include/pqxx")
add_executable(${PROJECT_NAME} ${SOURCES})
add_library(${PROJECT_NAME}_lib STATIC ${SOURCES})
# Linking
target_link_libraries(${PROJECT_NAME} ${LISTENERLIB} ${UUID} ${PQXX_LIB} ${PQ_LIB} Threads::Threads)
target_link_libraries(${PROJECT_NAME}_lib ${LISTENERLIB} ${UUID} ${PQXX_LIB} ${PQ_LIB} Threads::Threads)

# Includes
target_include_directories(${PROJECT_NAME} PRIVATE
    "include"
 )
target_include_directories(${PROJECT_NAME}_lib PRIVATE
    "include"
 )

