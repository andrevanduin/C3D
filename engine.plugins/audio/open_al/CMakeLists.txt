
cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE C3DOpenAL_SRC "*.h" "*.cpp")

add_library(C3DOpenAL SHARED ${C3DOpenAL_SRC})

target_compile_definitions(C3DOpenAL PUBLIC C3D_EXPORT)

target_link_libraries(C3DOpenAL PUBLIC C3DEngineRuntime)
target_link_libraries(C3DOpenAL PUBLIC OpenAL)

target_include_directories(C3DOpenAL PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(C3DOpenAL PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(C3DOpenAL PUBLIC "${OpenAL_SOURCE_DIR}/include")