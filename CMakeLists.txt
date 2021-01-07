cmake_minimum_required(VERSION 3.17)
project(ComPorstLinux C)

set(CMAKE_C_STANDARD 99)
add_compile_options(-lm)
add_executable(ComPorstLinux main.c)