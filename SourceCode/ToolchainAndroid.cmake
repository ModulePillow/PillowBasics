   if(CMAKE_ANDROID_ARCH_ABI NOT STREQUAL "arm64-v8a")
      message(FATAL_ERROR "Only support arm64-v8a in Android, but CMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}.")
   endif()
   # TODO