cmake_minimum_required(VERSION 2.8.5)
project(rawmedia)

if (${CMAKE_C_COMPILER_ID} MATCHES GNU)
  set(CMAKE_C_FLAGS "-std=c99 -Wall")
endif (${CMAKE_C_COMPILER_ID} MATCHES GNU)

add_subdirectory(rawmedia)
add_subdirectory(example)
