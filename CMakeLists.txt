cmake_minimum_required(VERSION 3.14)
project(untitled C)

set(CMAKE_C_STANDARD 99)

add_executable(receiver receiver.c)
add_executable(sender sender.c)