# Contributing to ImGui DirectX12 Kiero Hook

Thank you for your interest in the project! This document contains guidelines for contributing to the project development.

## Table of Contents

- [How to Contribute](#how-to-contribute)
- [Development Environment Setup](#development-environment-setup)
- [Code Style](#code-style)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Creating Issues](#creating-issues)
- [Creating Pull Requests](#creating-pull-requests)

## How to Contribute

There are several ways to contribute to the project:

1. **Report a bug** - create an Issue describing the problem
2. **Suggest a new feature** - create an Issue describing your idea
3. **Fix a bug** - create a Pull Request with the fix
4. **Add a new feature** - create a Pull Request with the implementation
5. **Improve documentation** - fix errors or add examples

## Development Environment Setup

### Requirements

- Windows 10/11 (x64)
- Visual Studio 2019/2022 or VSCode with CMake Tools
- CMake 3.24+
- Git
- DirectX 12 SDK

### Initial Setup

1. Fork the repository on GitHub
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/ImGui-DirectX-12-Kiero-Hook.git
   cd ImGui-DirectX-12-Kiero-Hook
   ```

3. Add the upstream repository:
   ```bash
   git remote add upstream https://github.com/chadlrnsn/ImGui-DirectX-12-Kiero-Hook.git
   ```

4. Clone submodules:
   ```bash
   git submodule update --init --recursive
   ```

5. Create a branch for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   # or for bug fixes:
   git checkout -b fix/issue-description
   ```

### Building the Project

1. Open the project in VSCode with CMake Tools installed
2. Select a suitable kit (VS2019/2022 x64)
3. Select configuration (Debug/Release)
4. Build the project: `Ctrl+Shift+P` → "CMake: Build"

## Code Style

### General Principles

- **C++20** - use modern language features
- **Readability** - code should be clear and self-documenting
- **Performance** - avoid unnecessary allocations and copies
- **Safety** - always check pointers and array bounds

### Naming Conventions

- **Classes**: `PascalCase` (e.g., `D3D12Hook`)
- **Functions**: `camelCase` (e.g., `createRenderTarget`)
- **Variables**: `camelCase` (e.g., `frameContext`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `NUM_FRAMES_IN_FLIGHT`)
- **Files**: `snake_case` (e.g., `d3d12hook.cpp`)

### Formatting

- **Indentation**: 4 spaces (no tabs)
- **Line length**: maximum 120 characters
- **Braces**: opening brace on the same line
- **Spaces**: around operators, after commas

```cpp
// ✅ Correct
if (condition) {
    doSomething();
    return result;
}

// ❌ Incorrect
if(condition)
{
    doSomething();
    return result;
}
```

### Comments

- Comment complex logic
- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Comment public API functions

```cpp
/**
 * Creates render target for the specified swap chain
 * @param swapChain Pointer to swap chain
 * @param device Pointer to D3D12 device
 * @return true if successful, false otherwise
 */
bool CreateRenderTarget(IDXGISwapChain3* swapChain, ID3D12Device* device);
```

## Testing

### Required Checks

Before submitting a Pull Request, ensure that:

1. **Code compiles** without errors and warnings
2. **Tested on both configurations**: Debug and Release
3. **Doesn't break existing functionality**
4. **Follows project style**

### Functional Testing

1. **DLL Build** - ensure DLL builds correctly
2. **Injection** - test injection into test application
3. **ImGui Interface** - verify UI displays correctly
4. **Performance** - ensure no FPS regressions

### Automated Checks

GitHub Actions automatically checks:
- Windows build
- Code style compliance
- Absence of critical errors

## Submitting Changes

### Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>: <short description>

<detailed description>

<footer>
```

**Types:**
- `feat:` - new feature
- `fix:` - bug fix
- `docs:` - documentation changes
- `style:` - formatting, missing semicolons, etc.
- `refactor:` - code refactoring
- `perf:` - performance improvements
- `test:` - adding or fixing tests
- `build:` - build system changes
- `ci:` - CI/CD changes
- `chore:` - other changes

**Examples:**
```
feat: add support for multiple render targets

fix: resolve memory leak in frame context cleanup

docs: update README with new build instructions
```

### Push and Pull Request

1. Commit your changes:
   ```bash
   git add .
   git commit -m "feat: add new feature description"
   ```

2. Push changes:
   ```bash
   git push origin feature/your-feature-name
   ```

3. Create a Pull Request on GitHub

## Creating Issues

### Bug Report Template

```markdown
**Bug Description**
Brief and clear description of the problem.

**Steps to Reproduce**
1. Open '...'
2. Click on '....'
3. Scroll down to '....'
4. See error

**Expected Behavior**
Description of what should happen.

**Screenshots**
If applicable, add screenshots.

**Environment**
- OS: [e.g., Windows 11]
- Windows Version: [e.g., 22H2]
- Project Version: [e.g., commit hash]

**Additional Context**
Any additional information.
```

### Feature Request Template

```markdown
**Description**
Brief description of the proposed feature.

**Problem**
Description of the problem this feature solves.

**Solution**
Description of the proposed solution.

**Alternatives**
Alternative solutions you considered.

**Additional Context**
Any additional information.
```

## Creating Pull Requests

### Preparation

1. **Update your branch** with latest changes from upstream:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Ensure all tests pass**

3. **Update documentation** if necessary

### Pull Request Description

```markdown
## Description
Brief description of changes.

## Type of Changes
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation improvement
- [ ] Refactoring
- [ ] Other

## Testing
Describe how you tested the changes.

## Checklist
- [ ] Code compiles without errors
- [ ] Tested in Debug and Release
- [ ] Documentation updated
- [ ] Tests added (if applicable)
- [ ] Follows project style

## Related Issues
Closes #123
```

### Code Review

After creating a Pull Request:

1. **Automated checks** must pass successfully
2. **Code review** from maintainers
3. **Fixes** if necessary
4. **Merge** after approval

## Contact

If you have questions:

- Create an Issue on GitHub
- Describe the problem in detail
- Attach error logs if available

## Acknowledgments

Thank you to all contributors for their contributions to the project development!

---

**Note**: This document may be updated. Follow the latest changes in the repository.
