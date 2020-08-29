cmake_minimum_required (VERSION 3.17)

project ("gmf_allocator")
set(CMAKE_CXX_STANDARD 20)

add_subdirectory("source")

file(GLOB_RECURSE gmf_allocator_src ${CMAKE_CURRENT_SOURCE_DIR}/source/gmf_memory_resource.h)
add_library(gmf_allocator INTERFACE)
target_include_directories(gmf_allocator INTERFACE ${gmf_allocator_src})
set(gmf_allocator_src ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")