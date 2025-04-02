cmake_minimum_required(VERSION 3.29)
project(Assembly_Project_1 C)

set(CMAKE_C_STANDARD 11)

add_executable(Assembly_Project_1
    z16sim.c
        z16asm.c
        z16asm2.c)
