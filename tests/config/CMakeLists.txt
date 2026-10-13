# Programs built from two files with different configurations, to check that
# lock_align notices. main.cpp and other.cpp each get the definitions given.
function(config_program name)
  cmake_parse_arguments(ARG "NO_TEST" "" "MAIN;OTHER" ${ARGN})
  add_library(${name}_other OBJECT other.cpp)
  target_link_libraries(${name}_other PRIVATE lock_align::lock_align)
  target_compile_definitions(${name}_other PRIVATE ${ARG_OTHER})
  lock_align_dev_target(${name}_other)
  add_executable(${name} main.cpp $<TARGET_OBJECTS:${name}_other>)
  target_link_libraries(${name} PRIVATE lock_align::lock_align)
  target_compile_definitions(${name} PRIVATE ${ARG_MAIN})
  lock_align_dev_target(${name})
  if(NOT ARG_NO_TEST)
    add_test(NAME "config: ${name}" COMMAND ${name})
  endif()
endfunction()

set(mismatch "lock_align: configuration mismatch between translation units")

config_program(same_configuration)
set_tests_properties("config: same_configuration" PROPERTIES PASS_REGULAR_EXPRESSION "^ran")

# These abort, which ctest counts as a failure whatever they print, so a
# script checks their output instead.
function(expect_mismatch name)
  config_program(${name} NO_TEST ${ARGN})
  add_test(NAME "config: ${name} is reported"
           COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:${name}> "-DEXPECT=${mismatch}"
                   -P ${PROJECT_SOURCE_DIR}/tests/expect_output.cmake)
endfunction()

expect_mismatch(mixed_disable OTHER LOCK_ALIGN_DISABLE)
expect_mismatch(mixed_slots OTHER LOCK_ALIGN_HELD_SLOTS_RESERVED=8)
expect_mismatch(mixed_policy OTHER LOCK_ALIGN_DEFAULT_POLICY=::lock_align::log_on_violation)
expect_mismatch(mixed_obligation_check OTHER LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK=unchecked)

# Two spellings of the same policy are the same configuration.
config_program(policy_spelling OTHER LOCK_ALIGN_DEFAULT_POLICY=lock_align::throw_on_violation)
set_tests_properties("config: policy_spelling" PROPERTIES PASS_REGULAR_EXPRESSION "^ran")

config_program(mixed_defaults_allowed
  MAIN LOCK_ALIGN_ALLOW_MIXED_DEFAULTS
  OTHER LOCK_ALIGN_ALLOW_MIXED_DEFAULTS LOCK_ALIGN_DEFAULT_POLICY=::lock_align::log_on_violation)
set_tests_properties("config: mixed_defaults_allowed" PROPERTIES PASS_REGULAR_EXPRESSION "^ran")

# Without the check, each file still keeps its own configuration.
config_program(mixed_unchecked
  MAIN LOCK_ALIGN_NO_CONFIG_CHECK EXPECT_OTHER_DISABLED
  OTHER LOCK_ALIGN_NO_CONFIG_CHECK LOCK_ALIGN_DISABLE)
set_tests_properties("config: mixed_unchecked" PROPERTIES PASS_REGULAR_EXPRESSION "^isolated")

# Programs that must fail to build. They're left out of the normal build; each
# test builds one and expects the failure it's there to show.
add_executable(config_link_mismatch EXCLUDE_FROM_ALL link_main.cpp link_other.cpp)
target_link_libraries(config_link_mismatch PRIVATE lock_align::lock_align)
target_compile_definitions(config_link_mismatch PRIVATE LOCK_ALIGN_NO_CONFIG_CHECK)
lock_align_dev_target(config_link_mismatch)
set_source_files_properties(link_other.cpp PROPERTIES COMPILE_DEFINITIONS LOCK_ALIGN_DISABLE)
add_test(NAME "config: a function taking a mismatched type doesn't link"
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target config_link_mismatch)
set_tests_properties("config: a function taking a mismatched type doesn't link" PROPERTIES
  # "Undefined symbols" or "undefined reference" from GNU-style linkers,
  # "unresolved external symbol" from MSVC's.
  PASS_REGULAR_EXPRESSION "[Uu]ndefined|unresolved external" RESOURCE_LOCK build_dir)

add_executable(config_changed_disable EXCLUDE_FROM_ALL changed_disable.cpp)
target_link_libraries(config_changed_disable PRIVATE lock_align::lock_align)
lock_align_dev_target(config_changed_disable)
add_test(NAME "config: LOCK_ALIGN_DISABLE can't change within a file"
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target config_changed_disable)
set_tests_properties("config: LOCK_ALIGN_DISABLE can't change within a file" PROPERTIES
  PASS_REGULAR_EXPRESSION "LOCK_ALIGN_DISABLE changed after the first lock_align header"
  RESOURCE_LOCK build_dir)
