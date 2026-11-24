cmake_minimum_required(VERSION 3.20)
project(ValwoodWindows LANGUAGES C CXX RC)
get_filename_component(VALWOOD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(SDK "${VALWOOD_ROOT}/build/windows/sdl")
set(SDL "${SDK}/SDL2-2.32.10/x86_64-w64-mingw32")
set(MIXER "${SDK}/SDL2_mixer-2.8.1/x86_64-w64-mingw32")
set(TTF "${SDK}/SDL2_ttf-2.24.0/x86_64-w64-mingw32")
set(DOUBLE_PRECISION ON CACHE BOOL "" FORCE)
foreach(option GENERATE_DEBUG_SYMBOLS INTERPROCEDURAL_OPTIMIZATION JPH_BUILD_SHARED_LIBS ENABLE_INSTALL
    DEBUG_RENDERER_IN_DEBUG_AND_RELEASE PROFILER_IN_DEBUG_AND_RELEASE ENABLE_OBJECT_STREAM
    TARGET_UNIT_TESTS TARGET_HELLO_WORLD TARGET_PERFORMANCE_TEST TARGET_SAMPLES TARGET_VIEWER
    JPH_USE_DX12 JPH_USE_VK JPH_USE_MTL JPH_USE_CPU_COMPUTE USE_SSE4_1 USE_SSE4_2
    USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD)
    set(${option} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${VALWOOD_ROOT}/third_party/jolt/Build" "${CMAKE_BINARY_DIR}/physics")
include("${VALWOOD_ROOT}/build/windows/sources.cmake")
function(valwood_target target)
    target_include_directories(${target} PRIVATE "${VALWOOD_ROOT}" "${VALWOOD_ROOT}/platform/windows/include"
        "${VALWOOD_ROOT}/third_party/imgui" "${VALWOOD_ROOT}/third_party/imgui/backends"
        "${VALWOOD_ROOT}/third_party/jolt" "${SDL}/include" "${SDL}/include/SDL2" "${MIXER}/include" "${TTF}/include")
    target_compile_definitions(${target} PRIVATE ENGINE_ENABLE_IMGUI=1
        IMGUI_IMPL_OPENGL_LOADER_CUSTOM NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
        _FILE_OFFSET_BITS=64 __USE_MINGW_ANSI_STDIO=1 JPH_DOUBLE_PRECISION NDEBUG)
    target_compile_options(${target} PRIVATE -include "${VALWOOD_ROOT}/platform/windows/compat.h")
    target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++)
    target_link_libraries(${target} PRIVATE Jolt bcrypt winpthread)
endfunction()
add_executable(rpg WIN32 ${VALWOOD_SOURCES} platform.cpp windows_gl.c valwood.rc)
valwood_target(rpg)
# The legacy renderer/editor keeps several bounded CPU banks on the main
# thread's stack. Windows' 1 MiB default is smaller than the Linux budget.
# Reserve address space lazily; this does not allocate 16 MiB per thread.
target_link_options(rpg PRIVATE "-Wl,--stack,16777216")
target_link_libraries(rpg PRIVATE mingw32 "${SDL}/lib/libSDL2main.a" "${SDL}/lib/libSDL2.dll.a"
    "${MIXER}/lib/libSDL2_mixer.dll.a" "${TTF}/lib/libSDL2_ttf.dll.a" opengl32)
set_source_files_properties("${VALWOOD_ROOT}/engine/world_physics.cpp" PROPERTIES
    COMPILE_OPTIONS "-fno-rtti;-msse2;-mfpmath=sse;-ffp-contract=off")
set(VALIDATOR_SRCS tools/validate_world_authoring.cpp engine/world_runtime_components.cpp
    engine/world_runtime_demo.cpp engine/world_animation.cpp engine/world_prefab.cpp engine/world_entity_graph.cpp
    engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_shader_graph.cpp engine/world_living.cpp
    engine/world_micro_policy.cpp engine/world_cinematics.cpp engine/world_regions.cpp engine/world_gameplay.cpp
    engine/world_npc_memory.cpp engine/world_physics.cpp)
list(TRANSFORM VALIDATOR_SRCS PREPEND "${VALWOOD_ROOT}/")
add_executable(valwood_project_validate ${VALIDATOR_SRCS} platform.cpp valwood.rc)
valwood_target(valwood_project_validate)
add_executable(windows_platform_test "${VALWOOD_ROOT}/tests/windows_platform_test.cpp"
    "${VALWOOD_ROOT}/engine/world_assets.cpp" "${VALWOOD_ROOT}/engine/world_studio.cpp"
    "${VALWOOD_ROOT}/engine/world_studio_tools.cpp" "${VALWOOD_ROOT}/engine/world_surface_authoring.cpp"
    "${VALWOOD_ROOT}/engine/world_shader_graph.cpp" "${VALWOOD_ROOT}/engine/world_entity_graph.cpp"
    "${VALWOOD_ROOT}/engine/world_project_checkpoint.cpp" "${VALWOOD_ROOT}/engine/world_editor_session.cpp"
    platform.cpp cpu_asset_impl.c valwood.rc)
valwood_target(windows_platform_test)
add_executable(windows_worker_runner "${VALWOOD_ROOT}/tests/windows_worker_runner.cpp" platform.cpp valwood.rc)
valwood_target(windows_worker_runner)
