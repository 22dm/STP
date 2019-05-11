cmake_minimum_required(VERSION 3.14)
project(untitled C)

set(CMAKE_C_STANDARD 90)

add_executable(newreceiver new/receiver.c)
add_executable(newsender new/sender.c)
add_executable(th new/th.c)
find_package(Threads)
target_link_libraries(th ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(newreceiver ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(newsender ${CMAKE_THREAD_LIBS_INIT})