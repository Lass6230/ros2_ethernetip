cmake_minimum_required(VERSION 3.8)
project(ethernetip_master)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(yaml_cpp_vendor REQUIRED)

# ---------------------------------------------------------------------------
# EIPScanner vendored build
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  EIPScanner
  GIT_REPOSITORY https://github.com/nimbuscontrols/EIPScanner.git
  GIT_TAG        master
)
# Disable examples/tests in the vendored build
set(EXAMPLE_ENABLED OFF CACHE BOOL "" FORCE)
set(TEST_ENABLED OFF CACHE BOOL "" FORCE)
# EIPScannerS is a static lib that will be linked into our shared lib,
# so it must be compiled with -fPIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Use FetchContent_Populate + add_subdirectory(EXCLUDE_FROM_ALL) so that
# EIPScanner's own install() rules are skipped entirely.
FetchContent_GetProperties(EIPScanner)
if(NOT eipscanner_POPULATED)
  FetchContent_Populate(EIPScanner)
  add_subdirectory(
    ${eipscanner_SOURCE_DIR}
    ${eipscanner_BINARY_DIR}
    EXCLUDE_FROM_ALL)
endif()

# ---------------------------------------------------------------------------
# ethernetip_master library
# ---------------------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED
  src/ethernetip_master.cpp
  src/device_connection.cpp
  src/assembly_map.cpp
  src/explicit_messaging.cpp
  src/watchdog.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

# EIPScanner headers – BUILD_INTERFACE only so they don't leak into the export
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
  $<BUILD_INTERFACE:${eipscanner_SOURCE_DIR}/src>
)

# Use keyword target_link_libraries instead of ament_target_dependencies
# (the latter uses the plain signature which conflicts with PRIVATE keyword)
target_link_libraries(${PROJECT_NAME} PUBLIC
  rclcpp::rclcpp
  rclcpp_lifecycle::rclcpp_lifecycle
  yaml-cpp
)

# Link the STATIC EIPScanner lib PRIVATE – baked into our .so, invisible
# in the exported target's INTERFACE_LINK_LIBRARIES.
target_link_libraries(${PROJECT_NAME} PRIVATE
  EIPScannerS
)

# Make EIPScanner headers available to downstream packages via install
install(
  DIRECTORY ${eipscanner_SOURCE_DIR}/src/
  DESTINATION include/${PROJECT_NAME}/EIPScanner
  FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  rclcpp
  rclcpp_lifecycle
  yaml_cpp_vendor
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)

  # Test fixtures path
  set(TEST_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")

  # --- AssemblyMap tests ---
  ament_add_gtest(test_assembly_map test/test_assembly_map.cpp)
  target_link_libraries(test_assembly_map ${PROJECT_NAME})

  # --- Watchdog tests ---
  ament_add_gtest(test_watchdog test/test_watchdog.cpp)
  target_link_libraries(test_watchdog ${PROJECT_NAME})

  # --- DeviceConnection tests ---
  ament_add_gtest(test_device_connection test/test_device_connection.cpp)
  target_link_libraries(test_device_connection ${PROJECT_NAME})

  # --- YAML loading tests ---
  ament_add_gtest(test_yaml_loading test/test_yaml_loading.cpp)
  target_link_libraries(test_yaml_loading ${PROJECT_NAME})
  target_compile_definitions(test_yaml_loading PRIVATE
    TEST_FIXTURES_DIR="${TEST_FIXTURES_DIR}")
endif()

ament_package()
