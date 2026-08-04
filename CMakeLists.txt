# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.20)

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PDB_VERSION "NONE" CACHE STRING "The version number for the PDB file")

project ("C3DEngine")

# Define our compile definitions that are relevant for all targets
add_compile_definitions(C3D_MEMORY_METRICS)
#add_compile_definitions(C3D_MEMORY_METRICS_POINTERS)
add_compile_definitions(C3D_MEMORY_METRICS_MALLOC)
#add_compile_definitions(C3D_MEMORY_METRICS_STACKTRACE)
add_compile_definitions(C3D_VULKAN_USE_CUSTOM_ALLOCATOR)
#add_compile_definitions(C3D_VULKAN_ALLOCATOR_TRACE)

add_compile_definitions(C3D_LOG_DEBUG C3D_LOG_ERROR C3D_LOG_WARN C3D_LOG_INFO)

# Include sub-projects.

add_subdirectory ("engine.core")
add_subdirectory ("engine.runtime")
add_subdirectory ("engine.plugins")

#add_subdirectory ("engine.tests")
#add_subdirectory ("engine.tools")

add_subdirectory ("game.testenv")