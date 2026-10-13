# A project that uses an installed lock_align through find_package, as a
# project outside this repository would. check_install.cmake builds it.
cmake_minimum_required(VERSION 3.21)
project(lock_align_consumer LANGUAGES CXX)

find_package(lock_align REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE lock_align::lock_align)
# Directly in the build directory, whatever the generator, so the script can
# find it.
set_target_properties(consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}>)
