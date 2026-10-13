# Configures lock_align from SOURCE_DIR with CONFIGURE_ARGS, installs it into
# a prefix under WORK_DIR, builds the project next to this script against that
# prefix with find_package, and checks that its program prints EXPECT.
# GENERATOR, TOOLSET, PLATFORM, CXX_COMPILER and CONFIG are the outer build's,
# so both builds match it.

function(run)
  execute_process(COMMAND ${ARGN} OUTPUT_VARIABLE out ERROR_VARIABLE out RESULT_VARIABLE result)
  if(NOT result STREQUAL "0")
    string(REPLACE ";" " " command "${ARGN}")
    message(FATAL_ERROR "failed (${result}): ${command}\n${out}")
  endif()
  set(out "${out}" PARENT_SCOPE)
endfunction()

if(NOT CONFIG)
  set(CONFIG Debug)
endif()
set(common -G ${GENERATOR} -DCMAKE_BUILD_TYPE=${CONFIG})
if(TOOLSET)
  list(APPEND common -T ${TOOLSET})
endif()
if(PLATFORM)
  list(APPEND common -A ${PLATFORM})
endif()
# Visual Studio generators choose the compiler from the toolset instead.
if(NOT GENERATOR MATCHES "^Visual Studio")
  list(APPEND common -DCMAKE_CXX_COMPILER=${CXX_COMPILER})
endif()

file(REMOVE_RECURSE ${WORK_DIR})
run(${CMAKE_COMMAND} -S ${SOURCE_DIR} -B ${WORK_DIR}/lock_align ${common}
    -DLOCK_ALIGN_BUILD_TESTS=OFF -DLOCK_ALIGN_BUILD_EXAMPLES=OFF ${CONFIGURE_ARGS})
run(${CMAKE_COMMAND} --install ${WORK_DIR}/lock_align --prefix ${WORK_DIR}/prefix
    --config ${CONFIG})
run(${CMAKE_COMMAND} -S ${CMAKE_CURRENT_LIST_DIR} -B ${WORK_DIR}/consumer ${common}
    -DCMAKE_PREFIX_PATH=${WORK_DIR}/prefix -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF)
run(${CMAKE_COMMAND} --build ${WORK_DIR}/consumer --config ${CONFIG})
run(${WORK_DIR}/consumer/consumer)
string(REPLACE "\r\n" "\n" out "${out}")  # as Windows prints it
if(NOT out MATCHES "^${EXPECT}\n$")
  message(FATAL_ERROR "expected '${EXPECT}', got:\n${out}")
endif()
