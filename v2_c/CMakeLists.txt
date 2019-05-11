cmake_minimum_required(VERSION 3.14)
project(untitled C)

set(CMAKE_C_STANDARD 90)

add_executable(receiver receiver.c)
add_executable(sender sender.c)
find_package(Threads)
target_link_libraries(receiver ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sender ${CMAKE_THREAD_LIBS_INIT})