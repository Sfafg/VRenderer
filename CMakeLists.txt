cmake_minimum_required(VERSION 3.30.1)
project(VRenderer)
set(CMAKE_CXX_STANDARD 23)

option(VRENDERER_BUILD_TESTS "Build tests" TRUE)

set(TESTS_ROOT "${PROJECT_SOURCE_DIR}/tests")
set(SRC_ROOT "${PROJECT_SOURCE_DIR}/src")
set(RESOURCES_ROOT "${PROJECT_SOURCE_DIR}/resources")
set(RESOURCES_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources")
set(SHADER_ROOT "${SRC_ROOT}/shaders")
set(SHADER_OUTPUT_DIR "${RESOURCES_OUTPUT_DIR}/shaders")

add_compile_options(-static-libstdc++)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
set(CMAKE_CXX_STANDARD 23)

file(MAKE_DIRECTORY ${TESTS_ROOT} ${SRC_ROOT} ${RESOURCES_ROOT} ${RESOURCES_OUTPUT_DIR} ${SHADER_ROOT} ${SHADER_OUTPUT_DIR})

# Library
include(FetchContent)
FetchContent_Declare(
  VGraphics
  GIT_REPOSITORY "https://github.com/Sfafg/VGraphics.git"
  GIT_TAG main
)
FetchContent_MakeAvailable(VGraphics)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG master
)
FetchContent_MakeAvailable(glm)

file(GLOB_RECURSE SRC "${SRC_ROOT}/*.cpp")
file(GLOB_RECURSE HEADER "${SRC_ROOT}/*.h")

add_library(VRenderer STATIC)
target_link_libraries(VRenderer PUBLIC VGraphics glm)
target_sources(VRenderer
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES ${HEADER}
)
target_precompile_headers(VRenderer PUBLIC "${VGraphics_SOURCE_DIR}/src/VG/VG.h")
target_include_directories(VRenderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(VRENDERER_BUILD_TESTS)
# SHADERS
    find_program(GLSLC_EXECUTABLE glslc)
    if(NOT GLSLC_EXECUTABLE)
        message(FATAL_ERROR "glslc not found. Set GLSLC_EXECUTABLE manually or add to PATH.")
    endif()

    file(GLOB_RECURSE SHADERS "${SHADER_ROOT}/*.vert" "${SHADER_ROOT}/*.frag" "${SHADER_ROOT}/*.comp")
    foreach(SHADER ${SHADERS})
        string(REPLACE "${SHADER_ROOT}/" "" SHADER_NAME "${SHADER}")
        add_custom_command(
            OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
            COMMAND "${GLSLC_EXECUTABLE}" "${SHADER}" -o "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
            DEPENDS "${SHADER}"
            COMMENT "Compiling Shader: ${SHADER_NAME}.spv"
        )
        list(APPEND SHADER_DEPEND "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    endforeach()

    add_custom_target(Shaders ALL DEPENDS ${SHADER_DEPEND})

# RESOURCES
    file(GLOB_RECURSE RESOURCES "${RESOURCES_ROOT}/*.*")

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

    FetchContent_Declare(
      glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG master
      CMAKE_CACHE_ARGS
        -DGLFW_BUILD_WAYLAND:BOOL=ON
        -DGLFW_BUILD_X11:BOOL=OFF
        -DGLFW_BUILD_DOCS:BOOL=OFF
    )
    FetchContent_MakeAvailable(glfw)

# VRenderer
    add_executable(Renderer ${TESTS_ROOT}/Source.cpp)
    target_link_libraries(Renderer PRIVATE VRenderer glfw glm)
    add_dependencies(Renderer Shaders Resources)

# RendererTest
    add_executable(RendererTest ${TESTS_ROOT}/RendererTest.cpp)
    target_link_libraries(RendererTest PRIVATE VRenderer glfw glm)
    add_dependencies(RendererTest Shaders Resources)
endif()
