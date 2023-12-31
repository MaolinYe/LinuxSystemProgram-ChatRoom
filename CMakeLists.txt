cmake_minimum_required(VERSION 3.12)
project(SystemProgram C)

set(CMAKE_C_STANDARD 99)

add_executable(SystemProgram chatserver_yml.c chatclient_yml.c minIni.c main.c)
