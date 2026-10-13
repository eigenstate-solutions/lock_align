foreach(name layered_locks thread_pool waiting_on_threads custom_policy)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE lock_align::lock_align)
  lock_align_dev_target(${name})
endforeach()

foreach(name layered_locks thread_pool waiting_on_threads)
  add_test(NAME "example: ${name}" COMMAND ${name})
endforeach()

# custom_policy exits 1 to report the violation it is there to show, so a
# script checks that it failed and that it reported that one violation only.
add_test(NAME "example: custom_policy"
         COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:custom_policy>
                 "-DEXPECT=^1 violation\\(s\\)\n  lock order violation: blocking on mutex 'accounts' \\(level 1\\) while holding mutex 'audit log' \\(level 2\\)\n$"
                 -DEXPECT_FAILURE=ON
                 -P ${PROJECT_SOURCE_DIR}/tests/expect_output.cmake)
