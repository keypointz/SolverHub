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
set(GeoHub "D:/TEAM/EMPCommon/GeoHub")
set(MeshHub "D:/TEAM/EMPCommon/MeshHub")
set(MeshSolver "D:/TEAM/EMPCommon/MeshSolver")

# Find Boost package
find_package(Boost COMPONENTS system filesystem thread program_options regex chrono date_time atomic)

# If Boost is not found, we'll just use the include directories and manually specify libraries
if(NOT Boost_FOUND)
    message(STATUS "Boost not found via find_package, using manual include paths and libraries")
    set(Boost_INCLUDE_DIRS "D:/TEAM/BuildTry2/FreeCADLibs_12.1.2_x64_VC15/include")
    set(BOOST_LIBRARYDIR "D:/TEAM/BuildTry2/FreeCADLibs_12.1.2_x64_VC15/lib")

    # Define Debug and Release Boost libraries
    set(Boost_LIBRARIES_DEBUG
        "${BOOST_LIBRARYDIR}/boost_filesystem-vc141-mt-gd-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_program_options-vc141-mt-gd-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_regex-vc141-mt-gd-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_system-vc141-mt-gd-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_thread-vc141-mt-gd-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_chrono-vc141-mt-gd-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_date_time-vc141-mt-gd-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_atomic-vc141-mt-gd-x64-1_67.lib"
    )

    set(Boost_LIBRARIES_RELEASE
        "${BOOST_LIBRARYDIR}/boost_filesystem-vc141-mt-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_program_options-vc141-mt-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_regex-vc141-mt-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_system-vc141-mt-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_thread-vc141-mt-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_chrono-vc141-mt-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_date_time-vc141-mt-x64-1_67.lib"
        "${BOOST_LIBRARYDIR}/boost_atomic-vc141-mt-x64-1_67.lib"
    )

    # Use generator expressions to select the appropriate libraries based on build configuration
    set(Boost_LIBRARIES
        $<$<CONFIG:Debug>:${Boost_LIBRARIES_DEBUG}>
        $<$<CONFIG:Release>:${Boost_LIBRARIES_RELEASE}>
    )
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
# Set up configuration-dependent library paths
set(GEOHUB_LIB_DEBUG "${GeoHub}/x64/Debug/GeoHub.lib")
set(GEOHUB_LIB_RELEASE "${GeoHub}/x64/Release/GeoHub.lib")
set(MESHHUB_LIB_DEBUG "${MeshHub}/x64/Debug/MeshHub.lib")
set(MESHHUB_LIB_RELEASE "${MeshHub}/x64/Release/MeshHub.lib")
set(MESHSOLVER_LIB_DEBUG "${MeshSolver}/x64/Debug/MeshSolver.lib")
set(MESHSOLVER_LIB_RELEASE "${MeshSolver}/x64/Release/MeshSolver.lib")

# Create generator expressions for configuration-dependent linking
target_link_libraries(SolverHub
    $<$<CONFIG:Debug>:${GEOHUB_LIB_DEBUG}>
    $<$<CONFIG:Release>:${GEOHUB_LIB_RELEASE}>
    $<$<CONFIG:Debug>:${MESHHUB_LIB_DEBUG}>
    $<$<CONFIG:Release>:${MESHHUB_LIB_RELEASE}>
    $<$<CONFIG:Debug>:${MESHSOLVER_LIB_DEBUG}>
    $<$<CONFIG:Release>:${MESHSOLVER_LIB_RELEASE}>
    ${Boost_LIBRARIES}
)

# Enable testing
enable_testing()

# Add tests directory
add_subdirectory(tests)