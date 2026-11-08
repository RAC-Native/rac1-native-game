# The example level's plugin (docs/custom-levels.md, "Code"): built by the setup from this source with the game's SDK.
cmake_minimum_required(VERSION 3.16)
project(example_plateau C)
include(${RAC1_SDK}/rac1_mod.cmake)
rac1_mod_plugin(example_plateau example_plugin.c)
