cmake_minimum_required(VERSION 2.8)
project(kserver)
set(LISTENERLIB "socket_listener")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(SOURCES "src/main.cpp" "/usr/local/lib/libpqxx.a" "include/server/libsocket_listener.a" "include/database/DatabaseConnection.cpp")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -std=c++17 -pthread -lpq")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
  "include",
  "minlog"
)
target_link_libraries(${PROJECT_NAME} Threads::Threads)

