# rac1-link-host: the console's receiver (../core) built for the PC, for the loopback tests.
#   cmake -S ports/wiiu/link/host -B <build dir> && cmake --build <build dir> -j 3
cmake_minimum_required(VERSION 3.13)
project(rac1_link_host C)

set(CMAKE_C_STANDARD 11)
find_package(Threads REQUIRED)
add_executable(rac1-link-host main.c ../core/link_server.c ../core/link_hash.c)
target_compile_options(rac1-link-host PRIVATE -Wall -Wextra -O2)
target_link_libraries(rac1-link-host PRIVATE Threads::Threads)
