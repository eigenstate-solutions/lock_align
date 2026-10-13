# The library needs 3.21, for PROJECT_IS_TOP_LEVEL; lock_align's own tests and
# examples need 3.24, which cmake/lock_align_dev.cmake checks. The upper end
# keeps the policies at 3.24's wherever CMake is new enough to have them.
cmake_minimum_required(VERSION 3.21...3.24)
project(lock_align VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(lock_align INTERFACE)
add_library(lock_align::lock_align ALIAS lock_align)
target_include_directories(lock_align INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(lock_align INTERFACE cxx_std_20)
# ordered_thread starts threads, which some platforms, such as glibc before
# 2.34, only support in programs linked for them.
find_package(Threads REQUIRED)
target_link_libraries(lock_align INTERFACE Threads::Threads)

# lock_align's configuration, applied to every file that links
# lock_align::lock_align. Setting it here, once, keeps files from disagreeing;
# a program whose files disagree aborts at startup.
option(LOCK_ALIGN_DISABLE "Compile lock_align's checks out" OFF)
set(LOCK_ALIGN_HELD_SLOTS_RESERVED "" CACHE STRING
    "Entries each thread can hold before its record allocates (empty: 32)")
set(LOCK_ALIGN_DEFAULT_POLICY "" CACHE STRING
    "Violation policy used when none is given (empty: ::lock_align::throw_on_violation)")
set(LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK "" CACHE STRING
    "required or unchecked: whether signalling needs the obligation (empty: required)")
set_property(CACHE LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK PROPERTY STRINGS "" required unchecked)

if(LOCK_ALIGN_DISABLE)
  target_compile_definitions(lock_align INTERFACE LOCK_ALIGN_DISABLE)
endif()
if(NOT LOCK_ALIGN_HELD_SLOTS_RESERVED STREQUAL "")
  if(NOT LOCK_ALIGN_HELD_SLOTS_RESERVED MATCHES "^[0-9]+$")
    message(FATAL_ERROR "LOCK_ALIGN_HELD_SLOTS_RESERVED must be a plain integer")
  endif()
  target_compile_definitions(lock_align INTERFACE
    LOCK_ALIGN_HELD_SLOTS_RESERVED=${LOCK_ALIGN_HELD_SLOTS_RESERVED})
endif()
if(NOT LOCK_ALIGN_DEFAULT_POLICY STREQUAL "")
  target_compile_definitions(lock_align INTERFACE
    LOCK_ALIGN_DEFAULT_POLICY=${LOCK_ALIGN_DEFAULT_POLICY})
endif()
if(NOT LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK STREQUAL "")
  if(NOT LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK MATCHES "^(required|unchecked)$")
    message(FATAL_ERROR "LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK must be required or unchecked")
  endif()
  target_compile_definitions(lock_align INTERFACE
    LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK=${LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK})
endif()

# Installs the headers and a package for find_package(lock_align). The
# installed target carries the configuration above, as this one does.
option(LOCK_ALIGN_INSTALL "Generate lock_align's install rules" ${PROJECT_IS_TOP_LEVEL})
if(LOCK_ALIGN_INSTALL)
  include(cmake/lock_align_install.cmake)
endif()

option(LOCK_ALIGN_BUILD_TESTS "Build lock_align tests" ${PROJECT_IS_TOP_LEVEL})
option(LOCK_ALIGN_BUILD_EXAMPLES "Build lock_align examples" ${PROJECT_IS_TOP_LEVEL})

if(LOCK_ALIGN_BUILD_TESTS OR LOCK_ALIGN_BUILD_EXAMPLES)
  include(cmake/lock_align_dev.cmake)
endif()

if(LOCK_ALIGN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(LOCK_ALIGN_BUILD_EXAMPLES)
  enable_testing()
  add_subdirectory(examples)
endif()
