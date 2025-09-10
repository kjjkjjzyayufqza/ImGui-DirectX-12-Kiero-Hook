@echo off
echo Building project...
cmake --build --preset windows-x64-release-2022

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful! Renaming DLL to ASI and copying to target directory...

set "DLL_PATH=.bin\Release\DirectX12-Example-ImGui.dll"
set "ASI_PATH=E:\OBHK0.3_v27\scripts\DirectX12-Example-ImGui.asi"

if exist "%DLL_PATH%" (
    copy "%DLL_PATH%" "%ASI_PATH%" /Y
    if %ERRORLEVEL% equ 0 (
        echo Successfully copied DLL as ASI to E:\OBHK0.3_v27\scripts\
    ) else (
        echo Failed to copy DLL to target directory!
    )
) else (
    echo DLL file not found at %DLL_PATH%
)

pause