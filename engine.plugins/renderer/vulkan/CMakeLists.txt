
cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE C3DVulkanRenderer_SRC "*.h" "*.cpp")

add_library(C3DVulkanRenderer SHARED ${C3DVulkanRenderer_SRC})

target_compile_definitions(C3DVulkanRenderer PUBLIC C3D_EXPORT)

target_link_libraries(C3DVulkanRenderer PUBLIC C3DEngineRuntime)

find_package(Vulkan REQUIRED)
set(SHADERC_LIB "$ENV{VULKAN_SDK}/Lib/shaderc_shared.lib")

target_link_libraries(C3DVulkanRenderer PUBLIC ${Vulkan_LIBRARIES} ${SHADERC_LIB})

target_include_directories(C3DVulkanRenderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(C3DVulkanRenderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(C3DVulkanRenderer PUBLIC ${Vulkan_INCLUDE_DIRS})