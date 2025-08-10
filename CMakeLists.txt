cmake_minimum_required(VERSION 3.30.1)
project(VRenderer)
set(CMAKE_CXX_STANDARD 23)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

set(TESTS_ROOT "${PROJECT_SOURCE_DIR}/tests")
set(SRC_ROOT "${PROJECT_SOURCE_DIR}/src")
set(RESOURCES_ROOT "${PROJECT_SOURCE_DIR}/resources")
set(RESOURCES_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources")
set(SHADER_COMPILER_DIR "C:/VulkanSDK/1.3.261.1/Bin/glslc.exe")
set(SHADER_ROOT "${SRC_ROOT}/shaders")
set(SHADER_OUTPUT_DIR "${RESOURCES_OUTPUT_DIR}/shaders")

set(CMAKE_EXE_LINKER_FLAGS "-static")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(ShowConsole FALSE)

set(SRC
    ${SRC_ROOT}/Renderer.cpp
    ${SRC_ROOT}/Material.cpp
    ${SRC_ROOT}/Mesh.cpp
)

set(SHADERS
    ${SHADER_ROOT}/Renderer.comp
    ${SHADER_ROOT}/ClearInstructions.comp
    ${SHADER_ROOT}/Reduction.comp
    ${SHADER_ROOT}/shader.vert
    ${SHADER_ROOT}/shader.frag
    ${SHADER_ROOT}/shader1.vert
    ${SHADER_ROOT}/shader1.frag
    ${SHADER_ROOT}/passthrough.vert
)

set(RESOURCES
    # ${RESOURCES_ROOT}/Bottle.obj
)

file(MAKE_DIRECTORY ${TESTS_ROOT} ${SRC_ROOT} ${RESOURCES_ROOT} ${RESOURCES_OUTPUT_DIR} ${SHADER_ROOT} ${SHADER_OUTPUT_DIR})

# SHADERS
foreach(SHADER ${SHADERS})
    string(REPLACE "${SHADER_ROOT}/" "" SHADER_NAME "${SHADER}")
    add_custom_command(
        OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
        COMMAND "${SHADER_COMPILER_DIR}" "${SHADER}" -I"${CMAKE_CURRENT_SOURCE_DIR}/include" -o "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
        DEPENDS "${SHADER}"
        COMMENT "Compiling Shader: ${SHADER_NAME}.spv"
    )
    list(APPEND SHADER_DEPEND "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SHADER_DEPEND})

# RESOURCES
foreach(RESOURCE ${RESOURCES})
    string(REPLACE "${RESOURCES_ROOT}/" "" RESOURCE_NAME "${RESOURCE}")
    add_custom_command(
        OUTPUT "${RESOURCES_OUTPUT_DIR}/${RESOURCE_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy "${RESOURCE}" "${RESOURCES_OUTPUT_DIR}/${RESOURCE_NAME}"
        DEPENDS "${RESOURCE}"
        COMMENT "Copying resource: ${RESOURCE_NAME}"
    )
    list(APPEND RESOURCE_DEPEND "${RESOURCES_OUTPUT_DIR}/${RESOURCE_NAME}")
endforeach()

add_custom_target(Resources ALL DEPENDS ${RESOURCE_DEPEND})

# VRenderer
if(ShowConsole)
    set(CMAKE_CXX_FLAGS "-mwindows")
    add_compile_definitions(MAIN=WinMain)
endif()

if(NOT ShowConsole)
    add_compile_definitions(MAIN=main)
endif()

add_executable(VRenderer ${SRC} ${TESTS_ROOT}/Source.cpp)

# include(FetchContent)
# FetchContent_Declare(
#     ECS
#     GIT_REPOSITORY https://github.com/Sfafg/ECS.git
#     GIT_TAG main
# )
# FetchContent_MakeAvailable(ECS)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(VRenderer PRIVATE VGraphics-d glfw3 )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_libraries(VRenderer PRIVATE VGraphics glfw3 )
endif()

target_precompile_headers(VRenderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/VG/VG.h")

target_include_directories(VRenderer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    # "${ECS_SOURCE_DIR}/src"
)
target_link_directories(VRenderer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/"
)
add_dependencies(VRenderer Shaders Resources)
