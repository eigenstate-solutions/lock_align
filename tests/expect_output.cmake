# Runs PROGRAM and passes if its output (stdout and stderr together) matches
# EXPECT, however it exits. ctest's own PASS_REGULAR_EXPRESSION still fails a
# program that aborts. With EXPECT_FAILURE set, the program must also fail:
# exit non-zero or be ended by a signal.
execute_process(COMMAND "${PROGRAM}" OUTPUT_VARIABLE out ERROR_VARIABLE out RESULT_VARIABLE result)
string(REPLACE "\r\n" "\n" out "${out}")  # as Windows prints it
if(NOT out MATCHES "${EXPECT}")
  message(FATAL_ERROR "expected output matching '${EXPECT}', got (${result}):\n${out}")
endif()
if(EXPECT_FAILURE AND "${result}" STREQUAL "0")
  message(FATAL_ERROR "expected the program to fail, but it exited 0:\n${out}")
endif()
