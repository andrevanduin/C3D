# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd26812")
endif()

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(PDB_VERSION "NONE" CACHE STRING "The version number for the PDB file")

project ("C3DEngine")

# Define our compile definitions that are relevant for all targets
add_compile_definitions(C3D_MEMORY_METRICS)
#add_compile_definitions(C3D_MEMORY_METRICS_POINTERS)
add_compile_definitions(C3D_MEMORY_METRICS_MALLOC)
#add_compile_definitions(C3D_MEMORY_METRICS_STACKTRACE)
add_compile_definitions(C3D_VULKAN_USE_CUSTOM_ALLOCATOR)

add_compile_definitions(C3D_LOG_DEBUG C3D_LOG_ERROR C3D_LOG_WARN C3D_LOG_INFO)

Include(FetchContent)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.1.1
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_Declare(
    OpenAL
    GIT_REPOSITORY https://github.com/kcat/openal-soft.git
    GIT_TAG 1.23.1
)
FetchContent_MakeAvailable(fmt spdlog glm OpenAL)

target_compile_definitions(spdlog PUBLIC SPDLOG_FMT_EXTERNAL)
target_include_directories(spdlog PUBLIC "${fmt_SOURCE_DIR}/include")

# Include sub-projects.

add_subdirectory ("engine.core")
add_subdirectory ("engine.runtime")
add_subdirectory ("engine.plugins")
#add_subdirectory ("engine.tests")
#add_subdirectory ("engine.tools")

add_subdirectory ("game.testenv")