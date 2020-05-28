cmake_minimum_required(VERSION 3.16)
project(server2)

set(CMAKE_CXX_STANDARD 20)

add_executable(server main.cpp server.cpp server.h)

include_directories("/usr/lib/x86_64-linux-gnu")
target_link_libraries(server /usr/lib/x86_64-linux-gnu/libpqxx-6.4.so)

find_package (Threads)
target_link_libraries (server ${CMAKE_THREAD_LIBS_INIT})

