# The arena's plugin (the menu, the challenges, Kalebo III's enemies and their paths, the flame vents, the rails): built
# by the setup from this source with the game's SDK.
cmake_minimum_required(VERSION 3.16)
project(night_arena C)
include(${RAC1_SDK}/rac1_mod.cmake)
rac1_mod_plugin(night_arena arena_plugin.c)
