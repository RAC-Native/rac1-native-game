# Third-party libraries downloaded at configure time (FetchContent), each pinned to a release
# and checked against its SHA-256. The libraries kept in third_party/ are listed in
# THIRD_PARTY_NOTICES.md with the reason they are kept there.
include(FetchContent)
# (the downloaded files get the extraction time, so that a changed URL rebuilds what uses them)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# (fetched sources are reused by later configures; a parallel build tree can share them with
#  -DFETCHCONTENT_BASE_DIR=<dir>, and FETCHCONTENT_FULLY_DISCONNECTED=ON builds offline)
set(FETCHCONTENT_QUIET ON)

# GLFW 3.4 (zlib): window, OpenGL context, keyboard / mouse / gamepad
FetchContent_Declare(glfw
    URL      https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

# GLM 1.0.1 (MIT): vector / matrix math (headers only)
FetchContent_Declare(glm
    URL      https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
    URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
    SOURCE_SUBDIR _no_cmake)

# Dear ImGui 1.91.4, docking branch (MIT): the menus' and the debug tools' widgets
FetchContent_Declare(imgui
    URL      https://github.com/ocornut/imgui/archive/refs/tags/v1.91.4-docking.tar.gz
    URL_HASH SHA256=7405bdaf304b77d6d03e6d17d1f31ca3586fa0c65a466fa1dd71b6ca6a222023
    SOURCE_SUBDIR _no_cmake)

# TinyGLTF 2.9.7 (MIT; bundles nlohmann/json, MIT, and stb_image, public domain / MIT)
FetchContent_Declare(tinygltf
    URL      https://github.com/syoyo/tinygltf/archive/refs/tags/v2.9.7.tar.gz
    URL_HASH SHA256=bf92fe53b86302f9415e77504ecd81a58659292ed04dffec873f5c71665046a0
    SOURCE_SUBDIR _no_cmake)

# miniaudio 0.11.25 (public domain / MIT-0): the audio output
FetchContent_Declare(miniaudio
    URL      https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
    URL_HASH SHA256=b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274
    SOURCE_SUBDIR _no_cmake)

# Vulkan-Headers 1.3.296 (Apache-2.0 / MIT): the Vulkan API headers
FetchContent_Declare(vulkan_headers
    URL      https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.3.296.tar.gz
    URL_HASH SHA256=e204e0b3c19f622d197df945737f5db913d6621830999b8578d34e80a7c90585
    SOURCE_SUBDIR _no_cmake)

# volk 1.3.296.0 (MIT): the Vulkan runtime loader (dlopen / LoadLibrary, no SDK needed to build)
FetchContent_Declare(volk
    URL      https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.3.296.0.tar.gz
    URL_HASH SHA256=8ffd0e81e29688f4abaa39e598937160b098228f37503903b10d481d4862ab85
    SOURCE_SUBDIR _no_cmake)

FetchContent_MakeAvailable(glfw glm imgui tinygltf miniaudio vulkan_headers volk)

add_library(glm_headers INTERFACE)
target_include_directories(glm_headers SYSTEM INTERFACE ${glm_SOURCE_DIR})

add_library(tinygltf_headers INTERFACE)
target_include_directories(tinygltf_headers SYSTEM INTERFACE ${tinygltf_SOURCE_DIR})

add_library(miniaudio_headers INTERFACE)
target_include_directories(miniaudio_headers SYSTEM INTERFACE ${miniaudio_SOURCE_DIR})

add_library(volk STATIC ${volk_SOURCE_DIR}/volk.c)
target_include_directories(volk SYSTEM PUBLIC ${volk_SOURCE_DIR} ${vulkan_headers_SOURCE_DIR}/include)
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)
if(NOT WIN32)
    target_link_libraries(volk PUBLIC ${CMAKE_DL_LIBS})
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glad glfw volk)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD=1 IMGUI_IMPL_VULKAN_USE_VOLK=1)
if(APPLE)  # (the Metal renderer's ImGui backend: docs/metal.md)
    target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm)
    set_source_files_properties(${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(imgui PUBLIC "-framework Metal" "-framework QuartzCore")
endif()
