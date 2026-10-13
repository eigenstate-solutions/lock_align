# Programs whose outcome ends the process, so they can't be Catch2 tests:
# expect_output.cmake runs each and checks what it printed.
function(expect_program name expect)
  cmake_parse_arguments(ARG "EXPECT_FAILURE" "" "" ${ARGN})
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE lock_align::lock_align)
  lock_align_dev_target(${name})
  add_test(NAME "program: ${name}"
           COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:${name}> "-DEXPECT=${expect}"
                   -DEXPECT_FAILURE=${ARG_EXPECT_FAILURE}
                   -P ${PROJECT_SOURCE_DIR}/tests/expect_output.cmake)
endfunction()

expect_program(abort_policy
  "lock_align: lock order violation: blocking on mutex 'low' \\(level 1\\) while holding mutex 'high'"
  EXPECT_FAILURE)
expect_program(log_policy
  "lock_align: lock order violation: blocking on mutex 'low'[^\n]*\nwent ahead holding 2")
# The message is the uncaught exception's what(), as std::terminate prints it.
expect_program(notify_in_destructor
  "missing obligation: notifying 'ready' \\(level 1\\) without holding its obligation"
  EXPECT_FAILURE)
