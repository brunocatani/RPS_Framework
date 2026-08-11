if(NOT DEFINED RPS_FRAMEWORK_SOURCE_DIR OR
   NOT DEFINED RPS_FRAMEWORK_BINARY_DIR OR
   NOT DEFINED RPS_FRAMEWORK_STAGE_DIR)
  message(FATAL_ERROR "Package consumer test inputs are required")
endif()

set(_consumer_source "${RPS_FRAMEWORK_SOURCE_DIR}/tests/package-consumer")
set(_consumer_binary "${RPS_FRAMEWORK_BINARY_DIR}/package-consumer")
file(REMOVE_RECURSE "${_consumer_binary}")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${_consumer_source}"
    -B "${_consumer_binary}"
    -G "Visual Studio 17 2022"
    -A x64
    "-DCMAKE_PREFIX_PATH=${RPS_FRAMEWORK_STAGE_DIR}"
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_output
  ERROR_VARIABLE _configure_error
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
    "Package consumer configure failed:\n${_configure_output}\n${_configure_error}")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    --build "${_consumer_binary}"
    --config Release
    --
    /m:1
    /p:CL_MPCount=2
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_output
  ERROR_VARIABLE _build_error
)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
    "Package consumer build failed:\n${_build_output}\n${_build_error}")
endif()
