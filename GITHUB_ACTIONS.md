# GitHub Actions CI/CD

This project uses GitHub Actions for continuous integration and continuous deployment (CI/CD). The workflows are defined in the `.github/workflows` directory.

## Available Workflows

### 1. CMake Build and Test (`cmake.yml`)

This is a simple workflow that builds and tests the project on Windows using the latest Visual Studio.

- **Trigger**: Runs on push to `main` or `master` branches, or on pull requests to these branches.
- **Actions**:
  - Checks out the code
  - Sets up MSBuild and Visual Studio Developer Command Prompt
  - Installs Boost and Eigen
  - Configures CMake
  - Builds the project
  - Runs tests

### 2. Comprehensive CI (`ci.yml`)

This is a more comprehensive workflow that can build and test the project on multiple platforms.

- **Trigger**: Runs on push to `main` or `master` branches, or on pull requests to these branches. Ignores changes to markdown files and docs.
- **Features**:
  - Matrix build strategy (can be configured for multiple platforms)
  - Uses Ninja for faster builds
  - Implements ccache for build caching
  - Uploads build artifacts
  - Can be triggered manually via workflow_dispatch

## How to Use

### Viewing Workflow Results

1. Go to the "Actions" tab in your GitHub repository
2. Select the workflow you want to view
3. Click on a specific workflow run to see details
4. You can view logs, download artifacts, and see test results

### Customizing Workflows

To customize the workflows:

1. Edit the YAML files in the `.github/workflows` directory
2. Commit and push your changes
3. The updated workflows will be used for subsequent runs

### Adding More Platforms

The `ci.yml` workflow is set up to support multiple platforms. To add Linux or macOS:

1. Uncomment the relevant sections in the matrix configuration
2. Uncomment the dependency installation steps for those platforms
3. Commit and push your changes

### Manual Triggering

The `ci.yml` workflow supports manual triggering:

1. Go to the "Actions" tab in your GitHub repository
2. Select the "CI" workflow
3. Click "Run workflow"
4. Select the branch you want to run the workflow on
5. Click "Run workflow"

## Troubleshooting

If a workflow fails:

1. Check the logs for error messages
2. Verify that all dependencies are correctly specified
3. Ensure that your CMake configuration works locally
4. Check that the paths to dependencies are correct

For more help, refer to the [GitHub Actions documentation](https://docs.github.com/en/actions).
