project("tlochat")
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
set(CMAKE_LEGACY_CYGWIN_WIN32 0)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
enable_testing()

get_directory_property(parent_directory PARENT_DIRECTORY)
if (parent_directory)
  message(STATUS "tlochat is not the top level project")
  set(c_flags ${CMAKE_C_FLAGS})
  set(c_flags_debug ${CMAKE_C_FLAGS_DEBUG})
else()
  message(STATUS "tlochat is the top level project")

  # done only on first configuration
  if (NOT CONFIGURED_ONCE)
    set(DEFAULT_C_FLAGS ${CMAKE_C_FLAGS} CACHE INTERNAL
        "Original CMAKE_C_FLAGS.")
    set(DEFAULT_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG} CACHE INTERNAL
        "Original CMAKE_C_FLAGS_DEBUG.")
    set(CONFIGURED_ONCE TRUE CACHE INTERNAL
        "A flag showing that CMake has configured at least once.")
  endif()

  set(c_flags ${DEFAULT_C_FLAGS})
  set(c_flags_debug ${DEFAULT_C_FLAGS_DEBUG})
endif()

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(additional_warning_flags "-std=c11 -pedantic -Wall -Wextra -Werror")
  set(additional_debug_flags "")
  #set(additional_debug_flags "-fsanitize=address -fsanitize=undefined -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  #set(additional_debug_flags "-fsanitize=memory -fsanitize=undefined -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
else()
  set(additional_warning_flags "")
  set(additional_debug_flags "")
endif()

set(CMAKE_C_FLAGS "${c_flags} ${additional_warning_flags}" CACHE STRING
    "Flags used by the compiler during all build types." FORCE)
set(CMAKE_C_FLAGS_DEBUG "${c_flags_debug} ${additional_debug_flags}" CACHE STRING
    "Flags used by the compiler during debug builds." FORCE)

add_subdirectory(tloc)
include_directories(tloc)
add_definitions(-D_POSIX_C_SOURCE=200809L)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(tlo)
add_executable(tlochat_server client_handlers.c tlochat_server.c)
target_link_libraries(tlochat_server ${CMAKE_THREAD_LIBS_INIT} tloc tlochat)
