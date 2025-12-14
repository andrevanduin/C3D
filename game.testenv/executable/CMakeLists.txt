
cmake_minimum_required (VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)

add_executable (TestEnv "src/main.cpp")

target_link_libraries(TestEnv PUBLIC C3DEngineRuntime)
target_compile_definitions(TestEnv PRIVATE GLM_FORCE_XYZW_ONLY GLM_FORCE_QUAT_DATA_XYZW GLM_FORCE_QUAT_CTOR_XYZW)

if (WIN32)
	target_link_options(TestEnv PUBLIC /MANIFEST:NO)
endif()

add_custom_target(CopyEngineCoreDLL
	COMMAND ${CMAKE_COMMAND} -E copy 
	"${CMAKE_BINARY_DIR}/engine.core/${CMAKE_SHARED_LIBRARY_PREFIX}C3DEngineCore${CMAKE_SHARED_LIBRARY_SUFFIX}" 
	"${CMAKE_BINARY_DIR}/game.testenv/executable/"
	DEPENDS C3DEngineCore
)

add_custom_target(CopyEngineRuntimeDLL
	COMMAND ${CMAKE_COMMAND} -E copy 
	"${CMAKE_BINARY_DIR}/engine.runtime/${CMAKE_SHARED_LIBRARY_PREFIX}C3DEngineRuntime${CMAKE_SHARED_LIBRARY_SUFFIX}" 
	"${CMAKE_BINARY_DIR}/game.testenv/executable/"
	DEPENDS C3DEngineRuntime
)

add_custom_target(CopyVulkanRenderDLL
	COMMAND ${CMAKE_COMMAND} -E copy 
	"${CMAKE_BINARY_DIR}/engine.plugins/renderer/vulkan/${CMAKE_SHARED_LIBRARY_PREFIX}C3DVulkanRenderer${CMAKE_SHARED_LIBRARY_SUFFIX}" 
	"${CMAKE_BINARY_DIR}/game.testenv/executable/"
	DEPENDS C3DVulkanRenderer
)

add_custom_target(CopyGameDLL
	COMMAND ${CMAKE_COMMAND} -E copy
	"${CMAKE_BINARY_DIR}/game.testenv/lib/${CMAKE_SHARED_LIBRARY_PREFIX}TestEnvLib${CMAKE_SHARED_LIBRARY_SUFFIX}" 
	"${CMAKE_BINARY_DIR}/game.testenv/executable/"
	DEPENDS TestEnvLib
)

add_dependencies(TestEnv CopyEngineCoreDLL CopyEngineRuntimeDLL CopyVulkanRenderDLL CopyGameDLL)