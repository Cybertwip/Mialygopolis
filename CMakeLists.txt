cmake_minimum_required(VERSION 3.20)
project(Mialygopolis3D VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(M3D_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_program(ASSIMP_CLI assimp REQUIRED)

set(CUBIQUITY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cubiquity")
set(CHARACTER_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/character-assets")
set(GENERATED_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/assets")
set(AVATAR_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/source/avatar_presets.json")
set(AVATAR_CONVERTER "${CMAKE_CURRENT_SOURCE_DIR}/source/tools/build_avatar_assets.py")
set(CHARACTER_STUDIO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/CharacterStudio")

file(GLOB_RECURSE CHARACTER_ASSET_INPUTS CONFIGURE_DEPENDS
    "${CHARACTER_ASSET_ROOT}/*.vrm"
    "${CHARACTER_ASSET_ROOT}/*.json")

set(GENERATED_AVATARS
    "${GENERATED_ASSET_DIR}/catalog.m3c"
    "${GENERATED_ASSET_DIR}/catalog.json")

file(GLOB ANIMATION_INPUTS CONFIGURE_DEPENDS
    "${CHARACTER_STUDIO_ROOT}/public/lora-assets/animations/*.fbx")

add_custom_command(
    OUTPUT ${GENERATED_AVATARS}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_ASSET_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${AVATAR_CONVERTER}"
            --config "${AVATAR_CONFIG}"
            --asset-root "${CHARACTER_ASSET_ROOT}"
            --animation-root "${CHARACTER_STUDIO_ROOT}/public/lora-assets/animations"
            --assimp "${ASSIMP_CLI}"
            --output-dir "${GENERATED_ASSET_DIR}"
    DEPENDS "${AVATAR_CONVERTER}" "${AVATAR_CONFIG}" ${CHARACTER_ASSET_INPUTS} ${ANIMATION_INPUTS}
    COMMENT "Converting all CharacterStudio traits and kinematic FBX clips"
    VERBATIM)
add_custom_target(m3d_avatar_assets ALL DEPENDS ${GENERATED_AVATARS})

add_library(cubiquity_core STATIC
    "${CUBIQUITY_ROOT}/src/library/base.cpp"
    "${CUBIQUITY_ROOT}/src/library/cubiquity.cpp"
    "${CUBIQUITY_ROOT}/src/library/extraction.cpp"
    "${CUBIQUITY_ROOT}/src/library/geometry.cpp"
    "${CUBIQUITY_ROOT}/src/library/storage.cpp"
    "${CUBIQUITY_ROOT}/src/library/utility.cpp")
target_include_directories(cubiquity_core PUBLIC
    "${CUBIQUITY_ROOT}/src/library"
    "${CUBIQUITY_ROOT}/src/application/external")
if(APPLE OR MINGW)
    target_compile_definitions(cubiquity_core PRIVATE CUBIQUITY_USE_POOLSTL)
endif()

add_executable(Mialygopolis3D
    source/main.cpp
    source/app.cpp
    source/avatar_asset.cpp
    source/bindings.cpp
    source/character_catalog.cpp
    source/character_loadout.cpp
    source/game_session.cpp
    source/multiplayer_client.cpp
    source/renderer.cpp
    source/ui_renderer.cpp
    source/voxel_world.cpp
    "${CUBIQUITY_ROOT}/src/application/external/glad.c"
    "${CUBIQUITY_ROOT}/src/application/external/stb_image.c"
    "${CUBIQUITY_ROOT}/src/application/external/stb_image_write.c")
add_dependencies(Mialygopolis3D m3d_avatar_assets)

target_include_directories(Mialygopolis3D PRIVATE
    source
    "${CUBIQUITY_ROOT}/src/application/external"
    "${CUBIQUITY_ROOT}/src/library")
target_compile_definitions(Mialygopolis3D PRIVATE
    GL_SILENCE_DEPRECATION
    M3D_DEFAULT_ASSET_DIR="${GENERATED_ASSET_DIR}"
    M3D_CHARACTER_ASSET_ROOT="${CHARACTER_ASSET_ROOT}")
target_link_libraries(Mialygopolis3D PRIVATE cubiquity_core OpenGL::GL Threads::Threads)

if(TARGET SDL2::SDL2main)
    target_link_libraries(Mialygopolis3D PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(Mialygopolis3D PRIVATE SDL2::SDL2)

if(MSVC)
    target_compile_options(Mialygopolis3D PRIVATE /W4)
    target_compile_options(cubiquity_core PRIVATE /W2)
    if(M3D_WARNINGS_AS_ERRORS)
        target_compile_options(Mialygopolis3D PRIVATE /WX)
    endif()
else()
    target_compile_options(Mialygopolis3D PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cubiquity_core PRIVATE -Wall -Wextra -Wpedantic)
    if(M3D_WARNINGS_AS_ERRORS)
        target_compile_options(Mialygopolis3D PRIVATE -Werror)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME native_smoke
        COMMAND Mialygopolis3D --smoke-test --asset-dir "${GENERATED_ASSET_DIR}" --source-assets "${CHARACTER_ASSET_ROOT}")
endif()

install(TARGETS Mialygopolis3D RUNTIME DESTINATION .)
install(DIRECTORY "${GENERATED_ASSET_DIR}/" DESTINATION assets)
