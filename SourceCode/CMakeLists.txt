# Requirements.
cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

# Solution-wide & platform-wide settings.
# Toolchain files are applied before cmake run.
#if(WIN32)
#elseif(ANDROID)
#else()
#   message(FATAL_ERROR "Invalid target platform.")
#endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "Available configuration types" FORCE)
add_compile_definitions($<$<CONFIG:Debug>:PILLOW_DEBUG>) # A customized macro.

# Projects.
project(PillowBasics LANGUAGES CXX C)
add_subdirectory(Pillow)
add_subdirectory(3rdParty)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Pillow)