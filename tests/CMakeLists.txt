# The tests choose their own configurations, target by target.
if(LOCK_ALIGN_DISABLE OR NOT LOCK_ALIGN_HELD_SLOTS_RESERVED STREQUAL ""
   OR NOT LOCK_ALIGN_DEFAULT_POLICY STREQUAL ""
   OR NOT LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK STREQUAL "")
  message(FATAL_ERROR "lock_align's tests need the configuration options left at their "
                      "defaults; set LOCK_ALIGN_BUILD_TESTS=OFF to build with others")
endif()

include(FetchContent)

# Use an installed Catch2 if there is one; otherwise download this release.
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.16.1
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS 3)
FetchContent_MakeAvailable(Catch2)

include(Catch)

# Builds one test file as a test program, optionally as a named variant with
# its own compile definitions. Each file includes the listener that checks
# nothing stays held, and each test's first tag names its file.
function(lock_align_test name)
  cmake_parse_arguments(ARG "" "VARIANT" "DEFINITIONS" ${ARGN})
  file(READ ${name}.cpp source)
  if(NOT source MATCHES "#include \"held_listener.hpp\"")
    message(FATAL_ERROR "${name}.cpp must include held_listener.hpp")
  endif()
  string(REGEX REPLACE "^test_" "" tag ${name})
  string(REGEX MATCHALL "TEST_CASE\\(" cases "${source}")
  string(REGEX MATCHALL "\"\\[${tag}\\]" tagged "${source}")
  list(LENGTH cases case_count)
  list(LENGTH tagged tagged_count)
  if(NOT case_count EQUAL tagged_count)
    message(FATAL_ERROR "${name}.cpp: every test's first tag must be [${tag}] "
                        "(${tagged_count} of ${case_count} are)")
  endif()

  if(ARG_VARIANT)
    string(REPLACE " " "_" suffix "${ARG_VARIANT}")
    set(target ${name}_${suffix})
    set(prefix "${name} (${ARG_VARIANT})")
  else()
    set(target ${name})
    set(prefix ${name})
  endif()
  add_executable(${target} ${name}.cpp)
  target_link_libraries(${target} PRIVATE lock_align::lock_align Catch2::Catch2WithMain)
  target_compile_definitions(${target} PRIVATE ${ARG_DEFINITIONS})
  lock_align_dev_target(${target})
  catch_discover_tests(${target} TEST_PREFIX "${prefix}: ")
endfunction()

# Tests of the default configuration.
set(default_tests
    test_level
    test_held
    test_violation
    test_mutex
    test_ordered_lock
    test_condition_variable
    test_thread
    test_promise
    test_latch
    test_obligation
    test_task_scope
    test_names)
foreach(name ${default_tests})
  lock_align_test(${name})
endforeach()

# Tests of one other configuration each.
lock_align_test(test_disabled DEFINITIONS LOCK_ALIGN_DISABLE)
lock_align_test(test_unchecked_default DEFINITIONS LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK=unchecked)
# This one checks the recipe in the config docs, which declares the policy
# before any lock_align header, so it defines LOCK_ALIGN_DEFAULT_POLICY itself.
lock_align_test(test_custom_default_policy)

# The default tests again in other configurations, wherever their
# expectations still hold there.
#
# With 0 or 1 reserved slots, almost every entry takes the heap path.
# test_names counts allocations, so it would see the record's.
set(slot_tests ${default_tests})
list(REMOVE_ITEM slot_tests test_names)
foreach(name ${slot_tests})
  lock_align_test(${name} VARIANT "0 slots" DEFINITIONS LOCK_ALIGN_HELD_SLOTS_RESERVED=0)
  lock_align_test(${name} VARIANT "1 slot" DEFINITIONS LOCK_ALIGN_HELD_SLOTS_RESERVED=1)
endforeach()

# With unchecked as the default obligation check, everything but the tests of
# that check still holds.
foreach(name test_level test_held test_mutex test_ordered_lock test_thread test_obligation
             test_task_scope test_names)
  lock_align_test(${name} VARIANT "unchecked default"
                  DEFINITIONS LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK=unchecked)
endforeach()

add_subdirectory(config)
add_subdirectory(programs)

# Installs lock_align and builds a separate project against it with
# find_package, once as configured by default and once with the checks off.
function(install_test name expect)
  add_test(NAME "install: ${name}"
           COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                   -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/install/${name}
                   -DGENERATOR=${CMAKE_GENERATOR} -DTOOLSET=${CMAKE_GENERATOR_TOOLSET}
                   -DPLATFORM=${CMAKE_GENERATOR_PLATFORM} -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
                   -DCONFIG=$<CONFIG> -DEXPECT=${expect} "-DCONFIGURE_ARGS=${ARGN}"
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/install/check_install.cmake)
endfunction()

install_test(default checked)
install_test(disabled unchecked -DLOCK_ALIGN_DISABLE=ON)
