cmake_minimum_required(VERSION 3.30.1)
project(VRenderer)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_LINKER_TYPE LLD)
set(CMAKE_CXX_STANDARD 23)

option(VRENDERER_BUILD_TESTS "Build tests" OFF)

set(TESTS_ROOT "${PROJECT_SOURCE_DIR}/tests")
set(SRC_ROOT "${PROJECT_SOURCE_DIR}/src")
set(RESOURCES_ROOT "${PROJECT_SOURCE_DIR}/resources")
set(RESOURCES_OUTPUT_DIR "${CMAKE_BINARY_DIR}/resources")
set(SHADER_ROOT "${SRC_ROOT}/shaders")
set(SHADER_OUTPUT_DIR "${RESOURCES_OUTPUT_DIR}/shaders")


file(MAKE_DIRECTORY ${TESTS_ROOT} ${SRC_ROOT} ${RESOURCES_ROOT} ${RESOURCES_OUTPUT_DIR} ${SHADER_ROOT} ${SHADER_OUTPUT_DIR})
include(FetchContent)

# Lib RESOURCES
file(GLOB_RECURSE LIB_RESOURCES "${RESOURCES_ROOT}/*.*")

foreach(RESOURCE ${LIB_RESOURCES})
    string(REPLACE "${RESOURCES_ROOT}/" "" RESOURCE_NAME "${RESOURCE}")
    add_custom_command(
        OUTPUT "${RESOURCES_OUTPUT_DIR}/${RESOURCE_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy "${RESOURCE}" "${RESOURCES_OUTPUT_DIR}/${RESOURCE_NAME}"
        DEPENDS "${RESOURCE}"
        COMMENT "Copying resource: ${RESOURCE_NAME}"
    )
    list(APPEND LIB_RESOURCE_DEPEND "${RESOURCES_OUTPUT_DIR}/${RESOURCE_NAME}")
endforeach()

add_custom_target(VRenderer_LibResources ALL DEPENDS ${RESOURCE_DEPEND})

# Lib SHADERS
find_program(GLSLC_EXECUTABLE glslc)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Set GLSLC_EXECUTABLE manually or add to PATH.")
endif()

file(GLOB_RECURSE LIB_SHADERS "${SHADER_ROOT}/*.comp")

foreach(SHADER ${LIB_SHADERS})
    string(REPLACE "${SHADER_ROOT}/" "" SHADER_NAME "${SHADER}")
    add_custom_command(
        OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
        COMMAND "${GLSLC_EXECUTABLE}" -g -O0 -Werror "${SHADER}" -o "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
        DEPENDS "${SHADER}" "${LIB_SHADER_DEPEND}"
        COMMENT "Compiling Shader: ${SHADER_NAME}.spv"
    )
    list(APPEND LIB_SHADER_DEPEND "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
endforeach()

add_custom_target(VRenderer_LibShaders ALL DEPENDS "${LIB_SHADER_DEPEND}")


set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZIP OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG master
)
FetchContent_MakeAvailable(assimp)

# Library
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

add_library(VRenderer_VRenderer STATIC)
add_dependencies(VRenderer_VRenderer VRenderer_LibShaders)
target_link_libraries(VRenderer_VRenderer PUBLIC VGraphics glm assimp)
target_sources(VRenderer_VRenderer
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES ${HEADER}
)
target_precompile_headers(VRenderer_VRenderer PUBLIC "${VGraphics_SOURCE_DIR}/src/VG/VG.h")
target_include_directories(VRenderer_VRenderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(VRENDERER_BUILD_TESTS)
# SHADERS
    file(GLOB_RECURSE SHADERS "${SHADER_ROOT}/*.vert" "${SHADER_ROOT}/*.frag")

    foreach(SHADER ${SHADERS})
        string(REPLACE "${SHADER_ROOT}/" "" SHADER_NAME "${SHADER}")
        add_custom_command(
            OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
            COMMAND "${GLSLC_EXECUTABLE}" -g -O0 -Werror "${SHADER}" -o "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv"
            DEPENDS "${SHADER}" "${SHADER_DEPEND}"
            COMMENT "Compiling Shader: ${SHADER_NAME}.spv"
        )
        list(APPEND SHADER_DEPEND "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    endforeach()

    add_custom_target(VRenderer_Shaders ALL DEPENDS "${SHADER_DEPEND}")

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

    add_custom_target(VRenderer_Resources ALL DEPENDS ${RESOURCE_DEPEND})

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
    add_executable(VRenderer_Renderer ${TESTS_ROOT}/Source.cpp)
    target_link_libraries(VRenderer_Renderer PRIVATE VRenderer_VRenderer glfw glm assimp)
    add_dependencies(VRenderer_Renderer VRenderer_Shaders VRenderer_Resources)

# RendererTest
    add_executable(VRenderer_RendererTest ${TESTS_ROOT}/RendererTest.cpp)
    target_link_libraries(VRenderer_RendererTest PRIVATE VRenderer_VRenderer glfw glm)
    add_dependencies(VRenderer_RendererTest VRenderer_Shaders VRenderer_Resources)
endif()
