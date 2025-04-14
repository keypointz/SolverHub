cmake_minimum_required(VERSION 3.14)
project(SolverHub VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add compile definitions
add_compile_definitions(
    _DEBUG
    UNICODE
    _UNICODE
    SOLVERHUB_EXPORTS
)

# Set paths for external libraries
set(EIGEN3_INCLUDE_DIR "D:/TEAM/EMPCOMMON/eigen-3.3.7")

# Find Boost package
find_package(Boost COMPONENTS system filesystem thread)

# If Boost is not found, we'll just use the include directories
if(NOT Boost_FOUND)
    message(STATUS "Boost not found via find_package, using manual include paths")
    set(Boost_INCLUDE_DIRS "D:/TEAM/BuildTry2/FreeCADLibs_12.1.2_x64_VC15/include")
    set(Boost_LIBRARIES "")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/code
    "D:/TEAM/EMPCOMMON"
    "D:/TEAM/EMPCOMMON/Exception"
    "D:/TEAM/EMPCOMMON/eigen-3.3.7"
    "D:/TEAM/BuildTry2/FreeCADLibs_12.1.2_x64_VC15/include"
    "D:/TEAM/EMPCommon/MeshHub/include"
    "D:/TEAM/EMPCommon/GeoHub/include"
    ${EIGEN3_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

# Collect source files
file(GLOB_RECURSE SOURCES
    "code/*.cpp"
)

# Create the library
add_library(SolverHub SHARED ${SOURCES})

# Link libraries
target_link_libraries(SolverHub
    ${Boost_LIBRARIES}
)

# Enable testing
enable_testing()

# Add tests directory
add_subdirectory(tests)