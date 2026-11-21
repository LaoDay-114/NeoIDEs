@echo off
setlocal enabledelayedexpansion

rem ---------------------------------------------------------------------------
rem  NeoIDEs - one-shot build script.
rem  Prefers clang++; falls back to MinGW g++. When both are present, clang++
rem  compiles with the MinGW-w64 runtime so no MSVC install is required.
rem  Double-click build.bat and the executable is written to
rem  .neoide\build\NeoIDEs.exe. No CMake required.
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
set "CXXLIBS=-nodefaultlibs -L"!MINGW!\lib" -L"!GCCLIB!" -lstdc++ -lmingw32 -lgcc -lmingwex -lmsvcrt -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32 -lpthread"
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
echo [NeoIDEs] Compiler: %CLANG%
echo [NeoIDEs] Target: MinGW-w64

:build
set "OUTDIR=.neoide\build"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo [NeoIDEs] Building...

"%CLANG%" %CXXARGS% -std=c++20 -O2 -DNDEBUG ^
    -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
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
    -o "%OUTDIR%\NeoIDEs.exe" ^
    -municode -mwindows ^
    %CXXLIBS% ^
    -ld2d1 -ldwrite -ldwmapi -lole32 -luuid -lshell32 -lcomdlg32 -luser32 -lgdi32 -limm32

if errorlevel 1 (
    echo.
    echo [NeoIDEs] Build failed.
    exit /b 1
)

echo.
echo [NeoIDEs] Build succeeded: %OUTDIR%\NeoIDEs.exe
if /I "%~1"=="run" (
    start "" "%OUTDIR%\NeoIDEs.exe"
)
endlocal
exit /b 0
