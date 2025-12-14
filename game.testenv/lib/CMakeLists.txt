
cmake_minimum_required (VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE TestEnvLib_SRC "*.h" "*.cpp")

add_library(TestEnvLib SHARED ${TestEnvLib_SRC})

if (WIN32)
    target_link_options(TestEnvLib PUBLIC /MANIFEST:NO)
    set_target_properties(TestEnvLib PROPERTIES PDB_NAME "TestEnvLib${RANDOM_PDB_VERSION}")
endif()

target_compile_definitions(TestEnvLib PUBLIC C3D_EXPORT)
target_compile_definitions(TestEnvLib PRIVATE GLM_FORCE_XYZW_ONLY)

target_link_libraries(TestEnvLib PUBLIC C3DEngineRuntime)

target_include_directories(TestEnvLib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_custom_command(TARGET TestEnvLib  COMMAND POST_BUILD ${CMAKE_COMMAND} -E copy 
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}TestEnvLib${CMAKE_SHARED_LIBRARY_SUFFIX}" 
    "${CMAKE_BINARY_DIR}/game.testenv/executable"
)