cmake_minimum_required(VERSION 3.10)
project(file_transmission)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} server_v6.c include/become_daemon.c)