# src/_devtools/ — openly-invented dev tools (per ADR 0003)
# Gated by the top-level RAC1_DEVTOOLS option. Excluded from
# "definitive" builds.

if(NOT RAC1_DEVTOOLS)
    return()
endif()

add_library(rac1_devtools STATIC
    dev_menu.cpp
    dev_hoverboard.cpp
    dev_profiler.cpp
    dev_unlocks.cpp
)
target_include_directories(rac1_devtools PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(rac1_devtools PUBLIC rac1_runtime rac1_game glfw imgui)
