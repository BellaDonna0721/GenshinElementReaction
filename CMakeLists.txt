cmake_minimum_required(VERSION 3.10)
project(GenshinElement LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_SOURCES
    src/core/World.cpp
)

set(GAMEPLAY_SOURCES
    src/gameplay/SpawnPreset.cpp
)

set(SYSTEM_SOURCES
    src/systems/MovementSystem.cpp
    src/systems/InputSystem.cpp
    src/systems/LifetimeSystem.cpp
    src/systems/CollisionSystem.cpp
    src/systems/ElementReactionSystem.cpp
    src/systems/OutputSystem.cpp
)

set(BINDING_SOURCES
    src/gameplay/wasm_api.cpp
)

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${GAMEPLAY_SOURCES}
    ${SYSTEM_SOURCES}
    ${BINDING_SOURCES}
)

add_executable(game ${ALL_SOURCES})

target_include_directories(game PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(game PROPERTIES
    SUFFIX ".js"
)

target_link_options(game PRIVATE
    "SHELL:-s WASM=1"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_NAME='GameModule'"
    "SHELL:-s EXPORTED_FUNCTIONS='[\"_wasm_init\",\"_wasm_update\",\"_wasm_set_input\",\"_wasm_get_render_data\",\"_wasm_get_render_count\",\"_wasm_reset\",\"_malloc\",\"_free\"]'"
    "SHELL:-s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"getValue\",\"HEAPF32\",\"HEAPU8\",\"HEAP32\"]'"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s TOTAL_STACK=1048576"
    "SHELL:-s INITIAL_MEMORY=33554432"
    "SHELL:--no-entry"
)

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE_DIR:game>/$<TARGET_FILE_BASE_NAME:game>.wasm
        ${CMAKE_SOURCE_DIR}/dist/game.wasm
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE_DIR:game>/$<TARGET_FILE_BASE_NAME:game>.js
        ${CMAKE_SOURCE_DIR}/dist/game.js
    COMMENT "Copying WASM artifacts to dist/"
)