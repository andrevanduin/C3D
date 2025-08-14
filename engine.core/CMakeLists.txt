
cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE C3DEngine_SRC "src/*.h" "src/*.cpp")

add_library(C3DEngineCore SHARED ${C3DEngine_SRC})

target_compile_definitions(C3DEngineCore PUBLIC C3D_EXPORT)

if (WIN32)
    target_compile_options(C3DEngineCore PRIVATE /EHsc /wd4002)
endif()

if (UNIX)
    find_package(X11 REQUIRED)
    message(STATUS "X11_FOUND = ${X11_FOUND}")
    message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
    message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")
    set(OS_SPECIFIC_LIBS xcb X11-xcb "${X11_LIBRARIES}")
endif()

target_link_libraries(C3DEngineCore PUBLIC fmt::fmt spdlog::spdlog glm::glm OpenAL::OpenAL "${OS_SPECIFIC_LIBS}")

target_include_directories(C3DEngineCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src" "${spdlog_SOURCE_DIR}/include" "${fmt_SOURCE_DIR}/include" "${glm_SOURCE_DIR}")

include_directories(SYSTEM "${spdlog_SOURCE_DIR}/include" "${fmt_SOURCE_DIR}/include" "${glm_SOURCE_DIR}")