
@echo off
echo =============================================
echo   StackTrace - Build Script
echo =============================================
echo.
 
:: Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [-] CMake not found. Please install CMake from https://cmake.org/download/
    exit /b 1
)
 
:: Check for Visual Studio
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo [-] MSVC compiler not found.
    echo [*] Please run this script from a Visual Studio Developer Command Prompt
    echo [*] Or install Visual Studio Build Tools from:
    echo [*] https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    exit /b 1
)
 
echo [+] Creating build directory...
if not exist "StackTrace\build" mkdir "StackTrace\build"
cd StackTrace\build
 
echo [+] Running CMake configuration...
cmake .. -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [-] CMake configuration failed
    exit /b 1
)
 
echo [+] Building...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [-] Build failed
    exit /b 1
)
 
echo.
echo =============================================
echo [+] Build successful!
echo [+] Executable: StackTrace\build\bin\Release\StackTrace.exe
echo =============================================
 