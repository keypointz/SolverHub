cmake_minimum_required(VERSION 3.14)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

# Configure GoogleTest to use dynamic runtime
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Add test executables
# add_executable(standalone_test standalone_test.cpp)
add_executable(LocalData_test LocalData_test.cpp)

# Set runtime library to match GoogleTest
# set_target_properties(standalone_test PROPERTIES
#   MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
# )
set_target_properties(LocalData_test PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# Link against GoogleTest only (no SolverHub dependency)
# target_link_libraries(standalone_test
#   gtest_main
# )

# Link against GoogleTest and SolverHub
target_link_libraries(LocalData_test
  gtest_main
  SolverHub
)

# Include directories
# target_include_directories(standalone_test PRIVATE
#   ${gtest_SOURCE_DIR}/include
#   ${gtest_SOURCE_DIR}
# )

target_include_directories(LocalData_test PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/code
  ${gtest_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}
)

# Add tests to CTest
include(GoogleTest)
# gtest_discover_tests(standalone_test)
gtest_discover_tests(LocalData_test)
