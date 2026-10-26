cmake_minimum_required(VERSION 3.13)
project(rac1_wiiu_hello C)

add_executable(rac1_wiiu_hello main.c)
target_compile_options(rac1_wiiu_hello PRIVATE -Wall -O2 -ffp-contract=off)
wut_create_rpx(rac1_wiiu_hello)
