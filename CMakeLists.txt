cmake_minimum_required(VERSION 4.2)

project(RPS_Framework VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not supported")
endif()

option(RPS_FRAMEWORK_BUILD_TESTS "Build deterministic framework tests" ON)
set(
  RPS_FRAMEWORK_STAGE_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/stage"
  CACHE PATH
  "Auto-deploy staging root used by custom-fast")

add_library(RPSAddresses STATIC
  src/AddressCatalog.cpp
)
add_library(RPS::Addresses ALIAS RPSAddresses)
set_target_properties(RPSAddresses PROPERTIES EXPORT_NAME Addresses)

add_library(RPSRuntime STATIC
  src/BodyAccess.cpp
  src/CollisionFilter.cpp
  src/GeneratorOutput.cpp
  src/HavokAllocator.cpp
  src/Memory.cpp
  src/NativeReference.cpp
  src/PhysicsApi.cpp
  src/PhysicsScale.cpp
  src/PhysicsTiming.cpp
  src/RuntimeModule.cpp
  src/Shape.cpp
  src/WorldAccess.cpp
)
add_library(RPS::Runtime ALIAS RPSRuntime)
set_target_properties(RPSRuntime PROPERTIES EXPORT_NAME Runtime)

foreach(target IN ITEMS RPSAddresses RPSRuntime)
  target_compile_features(${target} PUBLIC cxx_std_23)
  target_compile_options(${target} PRIVATE
    /sdl
    /utf-8
    /permissive-
    /Zc:__cplusplus
    /Zc:preprocessor
    /W4
  )
  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
endforeach()

target_link_libraries(RPSRuntime PUBLIC RPS::Addresses version.lib)

add_custom_target(RPSFrameworkStage ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${RPS_FRAMEWORK_STAGE_DIR}"
  COMMAND ${CMAKE_COMMAND}
          --install "${CMAKE_CURRENT_BINARY_DIR}"
          --config "$<CONFIG>"
          --prefix "${RPS_FRAMEWORK_STAGE_DIR}"
  DEPENDS RPSAddresses RPSRuntime
  COMMENT "Auto-deploying RPS Framework headers and libraries to the local stage"
  VERBATIM
)

if(RPS_FRAMEWORK_BUILD_TESTS)
  enable_testing()

  add_executable(RPSAddressCatalogTests tests/AddressCatalogTests.cpp)
  target_link_libraries(RPSAddressCatalogTests PRIVATE RPS::Addresses)

  add_executable(RPSGeneratorOutputTests tests/GeneratorOutputTests.cpp)
  target_link_libraries(RPSGeneratorOutputTests PRIVATE RPS::Runtime)

  add_executable(RPSCollisionFilterTests tests/CollisionFilterTests.cpp)
  target_link_libraries(RPSCollisionFilterTests PRIVATE RPS::Runtime)

  add_executable(RPSRuntimeModuleTests tests/RuntimeModuleTests.cpp)
  target_link_libraries(RPSRuntimeModuleTests PRIVATE RPS::Runtime)

  add_executable(RPSBodyAccessTests tests/BodyAccessTests.cpp)
  target_link_libraries(RPSBodyAccessTests PRIVATE RPS::Runtime)

  add_executable(RPSPhysicsPolicyTests tests/PhysicsPolicyTests.cpp)
  target_link_libraries(RPSPhysicsPolicyTests PRIVATE RPS::Runtime)

  add_executable(RPSShapeTests tests/ShapeTests.cpp)
  target_link_libraries(RPSShapeTests PRIVATE RPS::Runtime)

  foreach(target IN ITEMS
      RPSAddressCatalogTests
      RPSGeneratorOutputTests
      RPSCollisionFilterTests
      RPSRuntimeModuleTests
      RPSBodyAccessTests
      RPSPhysicsPolicyTests
      RPSShapeTests)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_options(${target} PRIVATE /utf-8 /permissive- /Zc:__cplusplus /W4)
    add_test(NAME ${target} COMMAND ${target})
  endforeach()

  find_program(RPS_PWSH_EXECUTABLE NAMES pwsh REQUIRED)
  add_test(
    NAME RPSSourceBoundaryTests
    COMMAND
      "${RPS_PWSH_EXECUTABLE}"
      -NoProfile
      -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/SourceBoundaryTests.ps1"
      -Root "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  add_test(
    NAME RPSPackageConsumerTests
    COMMAND
      "${CMAKE_COMMAND}"
      -DRPS_FRAMEWORK_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DRPS_FRAMEWORK_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DRPS_FRAMEWORK_STAGE_DIR=${RPS_FRAMEWORK_STAGE_DIR}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/PackageConsumerTests.cmake"
  )
endif()

install(TARGETS RPSAddresses RPSRuntime
  EXPORT RPSFrameworkTargets
  ARCHIVE DESTINATION lib
)
install(DIRECTORY include/RPS DESTINATION include)
install(EXPORT RPSFrameworkTargets
  FILE RPSFrameworkTargets.cmake
  NAMESPACE RPS::
  DESTINATION lib/cmake/RPSFramework
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RPSFrameworkConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/RPSFrameworkConfig.cmake"
  INSTALL_DESTINATION lib/cmake/RPSFramework
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/RPSFrameworkConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/RPSFrameworkConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/RPSFrameworkConfigVersion.cmake"
  DESTINATION lib/cmake/RPSFramework
)
