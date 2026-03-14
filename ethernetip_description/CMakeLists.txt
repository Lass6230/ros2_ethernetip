cmake_minimum_required(VERSION 3.8)
project(ethernetip_description)

find_package(ament_cmake REQUIRED)

# ---------------------------------------------------------------------------
# Install config files and xacro macros
# ---------------------------------------------------------------------------
install(
  DIRECTORY
    config/
  DESTINATION share/${PROJECT_NAME}/config
)

install(
  DIRECTORY
    urdf/
  DESTINATION share/${PROJECT_NAME}/urdf
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(test_xacro test/test_xacro.py
    TIMEOUT 30
  )
endif()

ament_package()
