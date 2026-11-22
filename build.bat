@echo off
setlocal enabledelayedexpansion

rem ---------------------------------------------------------------------------
rem  NeoIDEs - one-shot build script.
rem  Prefers clang++; falls back to MinGW g++. When both are present, clang++
rem  compiles with the MinGW-w64 runtime so no MSVC install is required.
rem  Double-click build.bat and the executable is written to
rem  .neoide\build\NeoIDEs.exe. No CMake required.
rem  Run build.bat test to build and execute the syntax tests.
rem ---------------------------------------------------------------------------

cd /d "%~dp0"

rem --------------------------------------------------------------- locate clang
set "CLANG="
if exist "D:\LLVM\bin\clang++.exe" set "CLANG=D:\LLVM\bin\clang++.exe"
if not defined CLANG if exist "%ProgramFiles%\LLVM\bin\clang++.exe" set "CLANG=%ProgramFiles%\LLVM\bin\clang++.exe"
if not defined CLANG if exist "%LOCALAPPDATA%\Programs\LLVM\bin\clang++.exe" set "CLANG=%LOCALAPPDATA%\Programs\LLVM\bin\clang++.exe"
if not defined CLANG for /f "delims=" %%I in ('where clang++.exe 2^>nul') do if not defined CLANG set "CLANG=%%I"

rem ----------------------------------------------------------- locate MinGW g++
set "GXX="
if exist "D:\w64devkit\w64devkit\bin\g++.exe" set "GXX=D:\w64devkit\w64devkit\bin\g++.exe"
if not defined GXX if exist "%USERPROFILE%\w64devkit\bin\g++.exe" set "GXX=%USERPROFILE%\w64devkit\bin\g++.exe"
if not defined GXX for /f "delims=" %%I in ('where g++.exe 2^>nul') do if not defined GXX set "GXX=%%I"

if defined CLANG goto pick_clang
if defined GXX goto pick_gxx

echo [NeoIDEs] Could not find clang++ or g++. Install LLVM or w64devkit.
exit /b 1

rem ---------------------------------------------------------- clang + MinGW
:pick_clang
set "CXXARGS="
set "CXXLIBS="
if not defined GXX goto clang_native
for %%I in ("%GXX%") do set "GXXDIR=%%~dpI"
for %%I in ("!GXXDIR!..") do set "MINGW=%%~fI"
set "GCCLIB="
for /d %%V in ("!MINGW!\lib\gcc\x86_64-w64-mingw32\*") do if exist "%%~fV\libgcc.a" set "GCCLIB=%%~fV"
if not defined GCCLIB goto clang_native
set "CXXARGS=--target=x86_64-w64-windows-gnu"
rem Static MinGW runtime: the deployed exe must not require libstdc++-6.dll,
rem libgcc_s_seh-1.dll or libwinpthread-1.dll on the target machine.
set "CXXLIBS=-L"!MINGW!\lib" -L"!GCCLIB!" -static -static-libgcc -static-libstdc++"
echo [NeoIDEs] Compiler: %CLANG%
echo [NeoIDEs] Target: MinGW-w64 (runtime from !MINGW!)
goto build

:clang_native
echo [NeoIDEs] Compiler: %CLANG%
echo [NeoIDEs] Target: clang default toolchain
goto build

rem ---------------------------------------------------------------- MinGW g++
:pick_gxx
set "CLANG=%GXX%"
set "CXXARGS="
rem Static runtime keeps the release build self-contained.
set "CXXLIBS=-static -static-libgcc -static-libstdc++"
echo [NeoIDEs] Compiler: %CLANG%
echo [NeoIDEs] Target: MinGW-w64

:build
set "OUTDIR=.neoide\build"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

rem ------------------------------------------------- compile embedded resources
set "WINDRES="
if not defined GXX goto skip_res
for %%I in ("%GXX%") do set "WINDRES=%%~dpIwindres.exe"
:skip_res
if not defined WINDRES if exist "D:\mingw64\bin\windres.exe" set "WINDRES=D:\mingw64\bin\windres.exe"
if not defined WINDRES for /f "delims=" %%I in ('where windres.exe 2^>nul') do if not defined WINDRES set "WINDRES=%%I"
if not defined WINDRES (
    echo [NeoIDEs] Could not find windres.exe. Resources cannot be embedded.
    exit /b 1
)
echo [NeoIDEs] Resources: %WINDRES%
"%WINDRES%" -i resources\neoide.rc -o "%OUTDIR%\neoide_res.o"
if errorlevel 1 (
    echo [NeoIDEs] Resource compilation failed.
    exit /b 1
)

echo [NeoIDEs] Building...

"%CLANG%" %CXXARGS% -std=c++20 -O2 -DNDEBUG ^
    -DUNICODE -D_UNICODE ^
    -Isrc ^
    src\main.cpp ^
    src\app.cpp ^
    src\app_input.cpp ^
    src\app_document.cpp ^
    src\app_tree.cpp ^
    src\app_editor.cpp ^
    src\app_ui.cpp ^
    src\app_build.cpp ^
    src\common.cpp ^
    src\renderer.cpp ^
    src\syntax.cpp ^
    src\document.cpp ^
    src\runner.cpp ^
    src\file_tree.cpp ^
    src\completion_data.cpp ^
    src\ai_completion.cpp ^
    "%OUTDIR%\neoide_res.o" ^
    -o "%OUTDIR%\NeoIDEs.exe" ^
    -municode -mwindows ^
    %CXXLIBS% ^
    -ld2d1 -ldwrite -ldwmapi -lole32 -luuid -lshell32 -lcomdlg32 -luser32 -lgdi32 -limm32 -lwinhttp -lwindowscodecs

if errorlevel 1 (
    echo.
    echo [NeoIDEs] Build failed.
    exit /b 1
)

echo.
echo [NeoIDEs] Build succeeded: %OUTDIR%\NeoIDEs.exe
if /I "%~1"=="test" (
    echo [NeoIDEs] Building syntax tests...
    "%CLANG%" %CXXARGS% -std=c++20 -O0 ^
        -DUNICODE -D_UNICODE ^
        -Isrc ^
        tests\syntax_tests.cpp ^
        src\common.cpp ^
        src\syntax.cpp ^
        src\completion_data.cpp ^
        src\ai_completion.cpp ^
        "%OUTDIR%\neoide_res.o" ^
        -o "%OUTDIR%\NeoIDEsSyntaxTests.exe" ^
        %CXXLIBS% ^
        -ld2d1 -ldwrite -ldwmapi -lole32 -luuid -lshell32 -lcomdlg32 -luser32 -lgdi32 -limm32 -lwinhttp
    if errorlevel 1 (
        echo [NeoIDEs] Syntax test build failed.
        exit /b 1
    )
    "%OUTDIR%\NeoIDEsSyntaxTests.exe"
    if errorlevel 1 exit /b 1
)
if /I "%~1"=="run" (
    start "" "%OUTDIR%\NeoIDEs.exe"
)
if /I "%~1"=="package" (
    echo.
    echo [NeoIDEs] Packaging runtime-only bundle...
    if exist ".neoide\dist" rmdir /s /q ".neoide\dist"
    mkdir ".neoide\dist"
    copy /y "%OUTDIR%\NeoIDEs.exe" ".neoide\dist\NeoIDEs.exe" >nul
    if errorlevel 1 (
        echo [NeoIDEs] Packaging failed: could not copy executable.
        exit /b 1
    )
    powershell -NoProfile -Command "Compress-Archive -Path '.\.neoide\dist\*' -DestinationPath '.\.neoide\NeoIDEs.zip' -Force"
    if errorlevel 1 (
        echo [NeoIDEs] Packaging failed: could not create archive.
        exit /b 1
    )
    echo [NeoIDEs] Package ready: .neoide\dist\NeoIDEs.exe
    echo [NeoIDEs] Archive ready: .neoide\NeoIDEs.zip
)
endlocal
exit /b 0
