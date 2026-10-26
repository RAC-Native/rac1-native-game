add_library(rac1_game STATIC
    math/vecmath.cpp
    lib/libc.cpp
    lib/libgraph.cpp
    lib/libvu0.cpp
    lib/snd989.cpp
    lib/libcdvd.cpp
    lib/movie_player.cpp
    lib/libmc.cpp
    lib/libgcc_fp.cpp
    ovl/level_change.cpp
    ovl/level_exit.cpp
    ovl/movies.cpp
    ovl/movie_mode.cpp
    ovl/memory_card.cpp
    ovl/memory_card_ops.cpp
    ovl/save_menus.cpp
    ovl/screens.cpp
    ovl/screen_fade.cpp
    ovl/vsync_callback.cpp
    ovl/ovl_1e_1f.cpp
    ovl/veldin_mobys.cpp
    ovl/ovl_20.cpp
    ovl/ovl_21.cpp
    ovl/ovl_22.cpp
    ovl/ovl_23.cpp
    ovl/ovl_24_25.cpp
    ovl/ovl_26_2a.cpp
    ovl/ovl_2b_2d.cpp
    ovl/ovl_2e.cpp
    ovl/ovl_dispatch.cpp
    ovl/hero_state.cpp
    ovl/hero_state_l1.cpp
    ovl/hero_state_update.cpp
    ovl/hero_state_update_l1.cpp
    ovl/hero_input.cpp
    ovl/hero_transitions.cpp
    ovl/hero_transitions_l1.cpp
    ovl/hero_l2_small.cpp
    ovl/hero_update.cpp
    ovl/variants_l1.cpp
    ovl/variants_l2.cpp
    ovl/variants_l3.cpp
    ovl/variants_l4.cpp
    ovl/variants_l5.cpp
    ovl/variants_l6.cpp
    ovl/variants_l7.cpp
    ovl/variants_l9.cpp
    ovl/variants_l8.cpp
    ovl/variants_l10.cpp
    ovl/variants_l11.cpp
    ovl/variants_l12.cpp
    ovl/variants_l13.cpp
    ovl/variants_l14.cpp
    ovl/variants_l15.cpp
    ovl/variants_l16.cpp
    ovl/variants_l17.cpp
    ovl/variants_l18.cpp
    ovl/eudora_mobys.cpp
    ovl/rilgar_mobys.cpp
    ovl/rilgar_hooks.cpp
    ovl/rilgar_part2.cpp
    ovl/umbris_part2.cpp
    ovl/veldin_extras.cpp
    ovl/weapon_classes.cpp
    ovl/blarg_mobys.cpp
    ovl/blarg_hooks.cpp
    ovl/blarg_part2.cpp
    ovl/umbris_mobys.cpp
    ovl/umbris_hooks.cpp
    ovl/gaspar_mobys.cpp
    ovl/gaspar_hooks.cpp
    ovl/batalia_mobys.cpp
    ovl/batalia_hooks.cpp
    ovl/batalia_part2.cpp
    ovl/orxon_mobys.cpp
    ovl/orxon_hooks.cpp
    ovl/pokitaru_hooks.cpp
    ovl/pokitaru_mobys.cpp
    ovl/hoven_hooks.cpp
    ovl/hoven_mobys.cpp
    ovl/gemlik_mobys.cpp
    ovl/gemlik_hooks.cpp
    ovl/oltanis_mobys.cpp
    ovl/oltanis_hooks.cpp
    ovl/levels13_14_shared.cpp
    ovl/quartu_hooks.cpp
    ovl/quartu_mobys.cpp
    ovl/kalebo_hooks.cpp
    ovl/kalebo_mobys.cpp
    ovl/veldin_orbit_hooks.cpp
    ovl/veldin_orbit_mobys.cpp
    ovl/veldin_finale_hooks.cpp
    ovl/veldin_finale_mobys.cpp
    ovl/end_menu.cpp
    ovl/particle_types4.cpp
    ovl/variants.cpp
    ovl/novalis_mobys.cpp
    ovl/aridia_mobys.cpp
    ovl/aridia_props.cpp
    ovl/kerwan_mobys.cpp
    ovl/kerwan_props.cpp
    ovl/kerwan_hooks.cpp
    ovl/aridia_hooks.cpp
    ovl/aridia_ocean.cpp
    ovl/novalis_platforms.cpp
    ovl/novalis_enemies.cpp
    ovl/novalis_npcs.cpp
    ovl/novalis_props.cpp
    ovl/novalis_mobys2.cpp
    ovl/novalis_hive.cpp
    ovl/novalis_cases.cpp
    ovl/novalis_teleporter.cpp
    ovl/novalis_path.cpp
    ovl/novalis_jet_base.cpp
    ovl/novalis_jets.cpp
    ovl/shared_classes_reg.cpp
    ovl/novalis_level_fns.cpp
    ovl/vendor_frame.cpp
    ovl/galactic_map.cpp
    ovl/sky_levels.cpp
    ovl/variants_sky.cpp
    ovl/checkpoint.cpp
    ovl/walker.cpp
    ovl/hero_l1_small.cpp
    ovl/hero_l1_ledge.cpp
    ovl/hero_l1_state.cpp
    ovl/hero_l1_input.cpp
    ovl/hero_l1_move.cpp
    ovl/hero_l1_misc.cpp
    ovl/level_setup.cpp
    boot/level_read.cpp
    boot/front_end.cpp
    boot/front_image.cpp
    boot/boot_start.cpp
    boot/title_load.cpp
    boot/title.cpp
    boot/title_menu.cpp
    ovl/level_load.cpp
    ovl/camera.cpp
    ovl/camera_follow.cpp
    ovl/overlay_init.cpp
    ovl/game_frame.cpp
    ovl/overlay_entry.cpp
    ovl/moby_update.cpp
    ovl/moby_classes.cpp
    ovl/ammo_pickup.cpp
    ovl/enemy_1440.cpp
    ovl/debris_piece.cpp
    ovl/sound.cpp
    ovl/music_streams.cpp
    ovl/render_passes.cpp
    ovl/hud.cpp
    ovl/stash.cpp
    ovl/hud_elements.cpp
    ovl/hud_o2.cpp
    ovl/quick_select.cpp
    ovl/gadgebot_commands.cpp
    enhance/advanced.cpp
    enhance/graphics.cpp
    enhance/controls.cpp
    enhance/update_notice.cpp
    ovl/bomb_glove.cpp
    ovl/pyrocitor.cpp
    ovl/blaster.cpp
    ovl/weapons_register.cpp
    ovl/mine_glove.cpp
    ovl/gadgets.cpp
    ovl/weapon_fx.cpp
    ovl/mine.cpp
    ovl/taunter.cpp
    ovl/doom_glove.cpp
    ovl/doom_agent.cpp
    ovl/carry_hooks.cpp
    ovl/carry_hooks_levels.cpp
    ovl/novalis_hooks.cpp
    ovl/novalis_water.cpp
    ovl/walloper.cpp
    ovl/weapon_draw.cpp
    ovl/ui_text.cpp
    ovl/help.cpp
    ovl/particles.cpp
    ovl/particle_16_spawn.cpp
    ovl/novalis_draw_cbs.cpp
    ovl/ship_draw_cbs.cpp
    ovl/empty_class_updates.cpp
    ovl/particles_draw.cpp
    ovl/moby_draw.cpp
    ovl/moby_pass.cpp
    ovl/moby_joints.cpp
    ovl/moby_shadows.cpp
    ovl/moby_vu1_run.cpp
    ovl/world_vu1.cpp
    ovl/ovl_reloc.cpp
    ovl/moby_vu0_reg.cpp
    ovl/particles_vu1.cpp
    ovl/shadows.cpp
    ovl/shadows_vu1.cpp
    ovl/sparkles_draw.cpp
    ovl/quad_draw.cpp
    ovl/glows.cpp
    ovl/effect_quads.cpp
    ovl/screen_effects.cpp
    ovl/pause_menu.cpp
    ovl/pause_menu_draw.cpp
    ovl/pause_menu_goals.cpp
    ovl/pause_menu_widgets.cpp
    ovl/pause_menu_options.cpp
    ovl/pause_menu_items.cpp
    ovl/pause_menu_models.cpp
    ovl/pause_menu_quick.cpp
    ovl/pause_menu_special.cpp
    ovl/planet_map.cpp
    ovl/planet_map_draw.cpp
    ovl/planet_missions.cpp
    ovl/pause_menu_help.cpp
    ovl/pause_menu_hero.cpp
    ovl/bomb.cpp
    ovl/effect_light.cpp
    ovl/camera_mods.cpp
    ovl/camera_look.cpp
    ovl/camera_types.cpp
    ovl/wind.cpp
    ovl/smoke_vent.cpp
    ovl/arrival.cpp
    ovl/flight_draw.cpp
    ovl/arrival_scene.cpp
    ovl/arrival_mobys.cpp
    ovl/cutscene.cpp
    ovl/talk.cpp
    ovl/nanotech.cpp
    ovl/nanotech_draw.cpp
    ovl/enemy_749.cpp
    ovl/render_setup.cpp
    input/pad_build.cpp
    render/render_toggles.cpp
    render/sky_render.cpp
    render/tfrag_render.cpp
    render/render_view.cpp
    render/world_draw.cpp
    render/orig_moby_draw.cpp
    render/moby_gpu_renderer.cpp
    render/mrp_scene.cpp
    render/mrp_moby_renderer.cpp
    render/mrp_world_assets.cpp
    render/mrp_world_check.cpp
    render/mrp_world_scene.cpp
    render/mrp_world_renderer.cpp
    render/mrp_world_static.cpp
    render/mrp_tfrag_gpu.cpp
    render/mrp_lod.cpp
    render/mrp_title_view.cpp
    render/mrp_fx.cpp
    render/replica_inventory.cpp
    render/mrp_shadows.cpp
    render/mrp_lighting.cpp
    render/tie_render.cpp
    render/shrub_render.cpp
    render/title_scene_host.cpp
    render/moby_render.cpp
    render/static_skinned_render.cpp
    render/shader_manager.cpp
    render/texture_loader.cpp
)
target_include_directories(rac1_game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rac1_game PUBLIC rac1_runtime tinygltf_headers)

# ---- the generated sources ----
# Part of the game's code is generated from the game's own MIPS and VU code, read from your disc image, by the
# generators in tools/codegen/ (docs/architecture.md, "The generated code"). It is not in the repository: ./setup.sh
# generates it into assets/generated/src/ (the same tree as src/), with generated_sources.cmake, the list of its files
# and the fingerprint of the generators and inputs that made them.
if(RAC1_NO_GENERATED)
    message(STATUS "RAC1_NO_GENERATED: the game's generated sources are left out (no executable is linked)")
    return()
endif()
set(RAC1_GENERATED_SRC "${CMAKE_SOURCE_DIR}/assets/generated/src" CACHE PATH "Where ./setup.sh generated the game's generated sources")
set(_rac1_gen_list "${RAC1_GENERATED_SRC}/generated_sources.cmake")
if(NOT EXISTS "${_rac1_gen_list}")
    message(FATAL_ERROR "The game's generated sources are missing (${_rac1_gen_list}).\n"
                        "Run ./setup.sh first: it generates them from your disc image, then builds the game.")
endif()
include("${_rac1_gen_list}")  # RAC1_GENERATED_SOURCES (paths under src/), RAC1_CODEGEN_FINGERPRINT
set(_rac1_gen_files "")
foreach(_f IN LISTS RAC1_GENERATED_SOURCES)
    if(NOT EXISTS "${RAC1_GENERATED_SRC}/${_f}")
        message(FATAL_ERROR "A generated source is missing: ${RAC1_GENERATED_SRC}/${_f}\n"
                            "Run ./setup.sh again: it regenerates the code.")
    endif()
    list(APPEND _rac1_gen_files "${RAC1_GENERATED_SRC}/${_f}")
endforeach()
target_sources(rac1_game PRIVATE ${_rac1_gen_files})
# The generated code without debug info: the same machine code (the objects' code is identical), about a third less time to
# compile it, a far smaller link and binary and less memory per compile job (the setup's job count follows the free memory).
# RAC1_GEN_DEBUG_INFO=ON keeps it, for stepping into the generated *_tr.cpp in a debugger.
option(RAC1_GEN_DEBUG_INFO "Compile the generated sources with debug info (the build type's -g)" OFF)
if(NOT RAC1_GEN_DEBUG_INFO AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${_rac1_gen_files} PROPERTIES COMPILE_OPTIONS "-g0")
endif()

# The generated code must match the generators and the headers it is built against: a `git pull` that changes them
# reconfigures the build (CMAKE_CONFIGURE_DEPENDS), and this check then asks for ./setup.sh, which regenerates the code.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/setup.py" --code-inputs
                OUTPUT_VARIABLE _rac1_gen_inputs RESULT_VARIABLE _rac1_r OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _rac1_r EQUAL 0)
    message(FATAL_ERROR "tools/setup.py --code-inputs failed")
endif()
string(REPLACE "\n" ";" _rac1_gen_inputs "${_rac1_gen_inputs}")
list(POP_FRONT _rac1_gen_inputs _rac1_fingerprint)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rac1_gen_list}" ${_rac1_gen_inputs})
if(NOT _rac1_fingerprint STREQUAL RAC1_CODEGEN_FINGERPRINT)
    message(FATAL_ERROR "The game's generated sources are out of date: the generators (tools/codegen/) or the headers "
                        "they read changed since they were generated.\n"
                        "Run ./setup.sh again: it regenerates the code, then builds the game.")
endif()
