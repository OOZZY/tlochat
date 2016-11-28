project("tlochat")
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
set(CMAKE_LEGACY_CYGWIN_WIN32 0)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
enable_testing()

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(warning_flags "-std=c11 -pedantic -Wall -Wextra -Werror")
  set(debug_flags "-g")
  #set(debug_flags "-g -fsanitize=address -fsanitize=undefined -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  #set(debug_flags "-g -fsanitize=memory -fsanitize=undefined -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
endif()

if (NOT CONFIGURED_ONCE)
  set(CMAKE_C_FLAGS "${warning_flags}" CACHE STRING
      "Flags used by the compiler during all build types." FORCE)
  set(CMAKE_C_FLAGS_DEBUG "${debug_flags}" CACHE STRING
      "Flags used by the compiler during debug builds." FORCE)
endif()

include_directories(tloc)
add_subdirectory(tloc)

add_definitions(-D_POSIX_C_SOURCE=200809L)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(tlo)
add_executable(tlochat_server client_handlers.c tlochat_server.c)
target_link_libraries(tlochat_server ${CMAKE_THREAD_LIBS_INIT} tloc tlochat)

set(CONFIGURED_ONCE TRUE CACHE INTERNAL
    "A flag showing that CMake has configured at least once.")
