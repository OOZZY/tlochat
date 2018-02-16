cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project("tlochat")
enable_testing()

# - tlocposix and tloc required
add_subdirectory(tlocposix)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(flags "-std=c11 -pedantic -Wall -Wextra -Werror")
  if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    set(flags "${flags} -Weverything -Wno-cast-align -Wno-padded -Wno-disabled-macro-expansion")
  endif()
  set(debug_flags "-g")
  #set(debug_flags "-g -fsanitize=address -fsanitize=undefined -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  #set(debug_flags "-g -fsanitize=memory -fsanitize=undefined -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
else()
  set(flags "${CMAKE_C_FLAGS}")
  set(debug_flags "${CMAKE_C_FLAGS_DEBUG}")
endif()

message(STATUS "tlochat setting flags")
set(CMAKE_C_FLAGS "${flags}" CACHE STRING
    "Flags used by the compiler during all build types." FORCE)
set(CMAKE_C_FLAGS_DEBUG "${debug_flags}" CACHE STRING
    "Flags used by the compiler during debug builds." FORCE)

add_subdirectory(src)
