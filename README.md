[![CMake on Windows](https://github.com/chadlrnsn/ImGui-DirectX-12-Kiero-Hook/actions/workflows/cmake-single-platform.yml/badge.svg)](https://github.com/chadlrnsn/ImGui-DirectX-12-Kiero-Hook/actions/workflows/cmake-single-platform.yml)

# ImGui DirectX12 Kiero Hook

A DirectX 12 hook implementation using ImGui and Kiero. This project allows you to inject custom ImGui interfaces into DirectX 12 applications.

> [!WARNING]
>
> **Anticheat detection risks:**  
> - **MinHook:** Anticheats may scan for `MinHook`, `MH_Initialize`, `MH_CreateHook`, or any `MH_` function names.  
> - **Kiero:** Can be detected by searching for `kiero`, `kiero::init`, `kiero::bind`.  
> - **DX12 hooks:** Anticheats often look for hooks on `Present`, `ExecuteCommandLists`, or vtable indices like 140 and 54.
>  
> Using these libraries or hooking methods can be flagged by anticheat systems.

## Features

- DirectX 12 Hook implementation with Present and ExecuteCommandLists interception
- ImGui integration through Kiero with DX12 backend
- Resizable window support with proper buffer management
- Fullscreen support with Alt+Enter handling
- Minimal performance impact using triple buffering
- Graceful injection handling (doesn't crash when injected before window creation)
- Console logging system for debugging
- Modern C++20 standard compliance

## Requirements

- Windows 10/11 (x64 only)
- Visual Studio 2019/2022 or VSCode with CMake Tools
- [DirectX 12 SDK](https://www.microsoft.com/en-us/download/details.aspx?id=6812)
- CMake 3.24 or higher
- Git

## Dependencies

The project automatically fetches and builds:
- **ImGui v1.91.7** - User interface framework
- **MinHook** - API hooking library
- **fmt 11.2.0** - Formatting library for logging
- **Kiero** - DirectX function hooking (included as vendor dependency)

## Project Structure

```
├── app/                    # Main DirectX 12 application
│   ├── src/               # Application source files
│   └── assets/            # Application assets
├── dll/                   # ImGui hook DLL
│   ├── src/               # DLL source files
│   │   ├── hooks/         # DirectX 12 hooking logic
│   │   └── dev/           # Development utilities (console, logger)
│   └── framework/         # Framework headers
├── vendor/                # Third-party dependencies
│   └── kiero/            # Kiero hooking library
└── cmake/                 # CMake configuration files
```

## Building the Project

### Using VSCode (Recommended)

1. Install the following extensions:
   - [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
   - [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)

2. Clone the repository with submodules:

```bash
git clone --recurse-submodules https://github.com/chadlrnsn/ImGui-DirectX-12-Kiero-Hook
cd ImGui-DirectX-12-Kiero-Hook
```

3. Open the project in VSCode and select a kit using CMake Tools:
   - `Visual Studio Build Tools 2019 Release - amd64`
   - `Visual Studio Build Tools 2022 Release - amd64`
   - `Clang x86_64-pc-windows-msvc`

4. Select build variant (Debug/Release) and build using CMake Tools

### Using Command Line

1. Clone the repository as shown above
2. Build using presets:

```bash
# For VS2019 Release
cmake --preset windows-x64-release-2019
cmake --build --preset windows-x64-release-2019

# For VS2019 Debug
cmake --preset windows-x64-debug-2019
cmake --build --preset windows-x64-debug-2019

# For VS2022 Release
cmake --preset windows-x64-release-2022
cmake --build --preset windows-x64-release-2022

# For VS2022 Debug
cmake --preset windows-x64-debug-2022
cmake --build --preset windows-x64-debug-2022
```

The output files will be in the `.bin/` directory:
- **Application**: `DirectX12-Example.exe`
- **DLL**: `DirectX12-Example-ImGui.dll`


## Known Issues

- Debug builds with debug layers enabled might crash
- Minor flickering may occur during window resizing
- Menu freezes when switching to fullscreen (Alt+Enter) while menu is open
- Limited to x64 architecture due to DirectX 12 requirements

## Troubleshooting

### Build Issues
- Ensure CMake 3.24+ is installed
- Verify DirectX 12 SDK is properly installed
- Check that all submodules are cloned (`git submodule update --init --recursive`)

### Runtime Issues
- DLL must be in the same directory as the executable
- Ensure target application uses DirectX 12
- Check console output for error messages

## Contributing

Feel free to submit issues and pull requests. When contributing:

1. Follow the existing code style (C++20, modern practices)
2. Test both Debug and Release builds
3. Ensure compatibility with Windows 10/11
4. Update documentation for new features

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
