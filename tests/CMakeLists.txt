cmake_minimum_required(VERSION 3.14)

# Find Google Test package
find_package(GTest QUIET)

# If GTest is not found, use FetchContent to download it
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Function to add a test
function(add_solver_test test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE 
        SolverHub
        GTest::gtest_main
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/$<CONFIG>
    )
endfunction()

# Add test files
add_solver_test(test_loadfromfile)
