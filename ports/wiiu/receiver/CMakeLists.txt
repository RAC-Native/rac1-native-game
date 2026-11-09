# rac1-native's Wii U receiver (main.cpp): our own code only - it can be released. A .wuhb for Aroma's menu, and the .rpx.
#   cmake -S ports/wiiu/receiver -B build-wiiu-receiver -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake && cmake --build build-wiiu-receiver
cmake_minimum_required(VERSION 3.13)
project(rac1_wiiu_receiver CXX)

add_executable(rac1_receiver main.cpp)
target_compile_features(rac1_receiver PRIVATE cxx_std_17)
target_compile_options(rac1_receiver PRIVATE -Wall -O2)
target_include_directories(rac1_receiver PRIVATE ${DEVKITPRO}/wums/include)
target_link_directories(rac1_receiver PRIVATE ${DEVKITPRO}/wums/lib)
target_link_libraries(rac1_receiver PRIVATE rpxloader)
wut_create_rpx(rac1_receiver)
wut_create_wuhb(rac1_receiver NAME "RAC1 Native Receiver" SHORTNAME "RAC1 Receiver" AUTHOR "rac1-native")
