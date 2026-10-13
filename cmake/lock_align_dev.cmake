# Build settings for lock_align's own tests, programs and examples. None of
# this touches the lock_align library target, so nothing here reaches the
# builds of projects that use lock_align.

# 3.24 for FetchContent_Declare's FIND_PACKAGE_ARGS, which uses an installed
# Catch2 before downloading one, and for the COMPILE_WARNING_AS_ERROR property.
if(CMAKE_VERSION VERSION_LESS 3.24)
  message(FATAL_ERROR "lock_align's tests and examples need CMake 3.24 or newer "
                      "(this is ${CMAKE_VERSION}); set LOCK_ALIGN_BUILD_TESTS=OFF and "
                      "LOCK_ALIGN_BUILD_EXAMPLES=OFF to use the library alone")
endif()

option(LOCK_ALIGN_WARNINGS_AS_ERRORS "Treat warnings as errors in lock_align's tests and examples"
       ON)
set(LOCK_ALIGN_SANITIZE "" CACHE STRING
    "Sanitizers for lock_align's tests and examples, as a list: address, undefined, thread")

# Which sanitizers this compiler can apply, checked once at configure time.
set(lock_align_sanitizers ${LOCK_ALIGN_SANITIZE})
foreach(sanitizer IN LISTS lock_align_sanitizers)
  if(NOT sanitizer MATCHES "^(address|undefined|thread)$")
    message(FATAL_ERROR
      "LOCK_ALIGN_SANITIZE: unknown sanitizer '${sanitizer}' (use address, undefined, thread)")
  endif()
endforeach()
if("thread" IN_LIST lock_align_sanitizers AND "address" IN_LIST lock_align_sanitizers)
  message(FATAL_ERROR "LOCK_ALIGN_SANITIZE: thread and address can't be combined")
endif()
if(MSVC AND lock_align_sanitizers AND NOT lock_align_sanitizers STREQUAL "address")
  message(FATAL_ERROR "LOCK_ALIGN_SANITIZE: with MSVC, only address is available")
endif()

# Applies warnings, warnings-as-errors and the chosen sanitizers to a target.
function(lock_align_dev_target target)
  if(MSVC)  # cl, and clang-cl, which takes cl's flags
    target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
  else()    # GCC, Clang and AppleClang
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  # -Werror or /WX as the compiler spells it; a build can still turn it off
  # with --compile-no-warning-as-error.
  set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR
                                             ${LOCK_ALIGN_WARNINGS_AS_ERRORS})
  get_target_property(type ${target} TYPE)
  if(MSVC AND type STREQUAL "EXECUTABLE")
    target_sources(${target} PRIVATE ${PROJECT_SOURCE_DIR}/cmake/no_error_dialogs.cpp)
  endif()

  if(NOT lock_align_sanitizers)
    return()
  endif()
  if(MSVC)
    target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
  else()
    list(JOIN lock_align_sanitizers "," list)
    target_compile_options(${target} PRIVATE -fsanitize=${list} -fno-omit-frame-pointer -g)
    target_link_options(${target} PRIVATE -fsanitize=${list})
    if("undefined" IN_LIST lock_align_sanitizers)
      # Undefined behavior fails the test instead of only printing.
      target_compile_options(${target} PRIVATE -fno-sanitize-recover=undefined)
    endif()
  endif()
endfunction()
