# SolverHub

[![CMake Build and Test](https://github.com/keypointz/SolverHub/actions/workflows/cmake.yml/badge.svg)](https://github.com/keypointz/SolverHub/actions/workflows/cmake.yml)
[![CI](https://github.com/keypointz/SolverHub/actions/workflows/ci.yml/badge.svg)](https://github.com/keypointz/SolverHub/actions/workflows/ci.yml)

## Development Environment Setup

### Prerequisites
- Visual Studio 2022 with C++ development workload
- CMake (version 3.14 or higher)
- Git
- VSCode with the following extensions:
  - C/C++ Extension Pack
  - CMake Tools
  - CodeLLDB (for debugging)
  - Augment (for AI-assisted coding)

### Building the Project

#### Using VSCode Tasks
1. Open the project in VSCode
2. Press `Ctrl+Shift+B` to run the default build task (Debug build)
3. Alternatively, open the Command Palette (`Ctrl+Shift+P`) and select "Tasks: Run Task" to choose a specific build task:
   - `cmake-configure`: Configure the CMake project
   - `build-debug`: Build the project in Debug mode
   - `build-release`: Build the project in Release mode
   - `clean`: Clean the build directory

#### Using CMake Directly
```bash
# Configure the project
cmake -B build -S . -G "Visual Studio 17 2022" -A x64

# Build in Debug mode
cmake --build build --config Debug

# Build in Release mode
cmake --build build --config Release

# Clean the build
cmake --build build --target clean
```

### Running Tests

#### Using VSCode Tasks
1. Open the Command Palette (`Ctrl+Shift+P`)
2. Select "Tasks: Run Task"
3. Choose "run-tests"

#### Using CMake Directly
```bash
# Run all tests
ctest --test-dir build -C Debug --output-on-failure
```

### Debugging

1. Open the source file you want to debug
2. Set breakpoints by clicking in the gutter or pressing F9
3. Press F5 to start debugging (select "MSVC Debug" configuration)
4. Use the debug toolbar to control execution:
   - Continue (F5)
   - Step Over (F10)
   - Step Into (F11)
   - Step Out (Shift+F11)
   - Restart (Ctrl+Shift+F5)
   - Stop (Shift+F5)

### Using Augment

1. Install the Augment extension from the VSCode marketplace
2. Sign in to your Augment account
3. Use the Augment panel to:
   - Ask questions about the codebase
   - Get code suggestions
   - Generate code based on natural language descriptions
   - Debug issues with AI assistance

## Continuous Integration

This project uses GitHub Actions for continuous integration. The workflows are defined in the `.github/workflows` directory.

### Available Workflows

- **CMake Build and Test**: A simple workflow that builds and tests the project on Windows.
- **Comprehensive CI**: A more comprehensive workflow that can build and test the project on multiple platforms.

For more details, see [GITHUB_ACTIONS.md](GITHUB_ACTIONS.md).

## Project Structure

- `code/`: Source code files
- `tests/`: Test files
- `datafiles/`: Data files for testing
- `.vscode/`: VSCode configuration files
- `.github/workflows/`: GitHub Actions workflow files
- `CMakeLists.txt`: Main CMake configuration file
