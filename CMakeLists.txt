cmake_minimum_required(VERSION 3.25)
project(NeoIDEs VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows resource compiler for the embedded completion database and app icon.
enable_language(RC)

set(NEOIDE_SOURCES
    resources/neoide.rc
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
    src/completion_data.cpp
    src/language_pack.cpp
)

# Tree-sitter runtime and language pack sources are downloaded into an ignored
# workspace folder; they are compiled statically into the single executable.
set(TS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party_downloads/tree_sitter")
file(GLOB TS_GRAMMAR_SOURCES CONFIGURE_DEPENDS "${TS_DIR}/grammars/*.c")
list(APPEND NEOIDE_SOURCES
    "${TS_DIR}/src/lib.c"
    "${TS_DIR}/generated_grammar_table.cpp"
    ${TS_GRAMMAR_SOURCES}
)

# A few grammars were generated for ABI 14 (`.version` / TSLexMode). Compile
# those against a small compatibility header so they link with the ABI 15
# runtime used by the rest of the language pack.
foreach(ts_source IN LISTS TS_GRAMMAR_SOURCES)
    # Some external scanners use assert() without including <assert.h>.
    set_property(SOURCE "${ts_source}" APPEND PROPERTY COMPILE_OPTIONS "-include;assert.h")
    file(READ "${ts_source}" ts_content)
    if(ts_content MATCHES "\\.version = LANGUAGE_VERSION")
        set_property(SOURCE "${ts_source}" APPEND PROPERTY
            INCLUDE_DIRECTORIES "${TS_DIR}/include_abi14")
    endif()
endforeach()

add_executable(NeoIDEs WIN32 ${NEOIDE_SOURCES})

target_compile_definitions(NeoIDEs PRIVATE
    UNICODE
    _UNICODE
)

target_include_directories(NeoIDEs PRIVATE
    src
    "${TS_DIR}/include"
    "${TS_DIR}/src"
)

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
    windowscodecs
)

if(MINGW)
    # wWinMain entry point, GUI subsystem, and a statically linked runtime so
    # the deployed exe needs no MinGW DLLs on the target machine.
    target_link_options(NeoIDEs PRIVATE -municode -mwindows -static -static-libgcc -static-libstdc++)
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

# Runtime-only bundle: exe + nothing else. Tests, sources, docs and object
# files stay in the build tree and are never copied into the package.
add_custom_target(package
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:NeoIDEs>" "${CMAKE_BINARY_DIR}/dist/"
    COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/dist"
            ${CMAKE_COMMAND} -E tar cf "${CMAKE_BINARY_DIR}/NeoIDEs.zip" --format=zip -- .
    DEPENDS NeoIDEs
    COMMENT "Packaging runtime-only NeoIDEs bundle"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(NeoIDEsSyntaxTests
        tests/syntax_tests.cpp
        src/common.cpp
        src/syntax.cpp
        src/completion_data.cpp
        src/language_pack.cpp
        "${TS_DIR}/src/lib.c"
        "${TS_DIR}/generated_grammar_table.cpp"
        ${TS_GRAMMAR_SOURCES}
        resources/neoide.rc
    )
    target_compile_definitions(NeoIDEsSyntaxTests PRIVATE
        UNICODE
        _UNICODE
    )
    target_include_directories(NeoIDEsSyntaxTests PRIVATE
        src
        "${TS_DIR}/include"
        "${TS_DIR}/src"
    )
    target_link_libraries(NeoIDEsSyntaxTests PRIVATE
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
    add_test(NAME NeoIDEsSyntaxTests COMMAND NeoIDEsSyntaxTests)
endif()
