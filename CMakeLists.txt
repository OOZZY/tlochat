cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project("tlochat")
enable_testing()

# - tlocposix and tloc required
add_subdirectory(tlocposix)

add_subdirectory(src)
