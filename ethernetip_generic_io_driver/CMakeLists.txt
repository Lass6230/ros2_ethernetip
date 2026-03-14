cmake_minimum_required(VERSION 3.8)
project(ethernetip_generic_io_driver)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(pluginlib REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(ethernetip_master REQUIRED)
find_package(ethernetip_device_driver_interface REQUIRED)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED
  src/generic_io_device.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

ament_target_dependencies(${PROJECT_NAME}
  pluginlib
  hardware_interface
  ethernetip_master
  ethernetip_device_driver_interface
)

# Register plugin
pluginlib_export_plugin_description_file(
  ethernetip_device_driver_interface
  ethernetip_generic_io_driver_plugin.xml)

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
  pluginlib
  hardware_interface
  ethernetip_master
  ethernetip_device_driver_interface
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_generic_io_device test/test_generic_io_device.cpp)
  target_link_libraries(test_generic_io_device ${PROJECT_NAME})
  ament_target_dependencies(test_generic_io_device
    ethernetip_master
    ethernetip_device_driver_interface
  )
endif()

ament_package()
