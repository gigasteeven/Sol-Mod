cmake_minimum_required(VERSION 3.21)
project(SolSpoutLayout VERSION 1.0.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "SOL Spout Layout is Windows-only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})

include(FetchContent)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    spout2
    GIT_REPOSITORY https://github.com/leadedge/Spout2.git
    GIT_TAG c974e48972f67bb10c48c8edb95d07d8d59eb18e
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(spout2)
target_link_libraries(${PROJECT_NAME} PRIVATE Spout_static opengl32)
target_include_directories(${PROJECT_NAME} PRIVATE ${spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL)
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX SPOUT_BUILD_STATIC)
target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /EHsc /W4)

if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK environment variable is not set")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})
