   # Containing two platforms in single solution is not supported by CMake. e.g. x64+Win32
   set(TARGET_WINDOWS_SDK 10.0.18362.0) #1903, 19H1
   set(CMAKE_SYSTEM_VERSION ${TARGET_WINDOWS_SDK}) # Where CMake runs.
   set(CMAKE_GENERATOR_PLATFORM x64,version=${TARGET_WINDOWS_SDK}) # Target machine.
   add_compile_definitions(_UNICODE)
   add_compile_options(/arch:SSE2 /Zc:__cplusplus) # Support SSE2, force the macro __cplusplus to return an updated value
   set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded") # Equal to /MD, using static C/C++ runtime library