cmake_minimum_required(VERSION 3.25)
project(NeoIDEs VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NEOIDE_SOURCES
    src/main.cpp
    src/app.cpp
    src/app_input.cpp
    src/app_document.cpp
    src/app_tree.cpp
    src/app_editor.cpp
    src/app_ui.cpp
    src/app_build.cpp
    src/common.cpp
    src/renderer.cpp
    src/syntax.cpp
    src/document.cpp
    src/runner.cpp
    src/file_tree.cpp
)

add_executable(NeoIDEs WIN32 ${NEOIDE_SOURCES})

target_compile_definitions(NeoIDEs PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_include_directories(NeoIDEs PRIVATE src)

target_link_libraries(NeoIDEs PRIVATE
    d2d1
    dwrite
    dwmapi
    ole32
    uuid
    shell32
    comdlg32
    user32
    gdi32
    imm32
)

if(MINGW)
    # wWinMain entry point plus a GUI subsystem binary.
    target_link_options(NeoIDEs PRIVATE -municode -mwindows)
    target_link_libraries(NeoIDEs PRIVATE stdc++ winpthread)
endif()

if(MSVC)
    target_compile_options(NeoIDEs PRIVATE /W4 /permissive- /utf-8)
    target_link_options(NeoIDEs PRIVATE /SUBSYSTEM:WINDOWS)
    set_target_properties(NeoIDEs PROPERTIES LINK_FLAGS "/ENTRY:wWinMainCRTStartup")
else()
    target_compile_options(NeoIDEs PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

set_target_properties(NeoIDEs PROPERTIES
    OUTPUT_NAME "NeoIDEs"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
