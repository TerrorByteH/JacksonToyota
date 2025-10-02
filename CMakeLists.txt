cmake_minimum_required(VERSION 3.21)

project(VSRM
    VERSION 0.1.0
    DESCRIPTION "Vehicle Service Records Manager (Toyota Zambia)"
    LANGUAGES CXX)

# Enable folders in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Add debug symbols
set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_BUILD_TYPE Debug)

# Enable IDE folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Detect if building with MSVC and set Unicode
if(MSVC)
    add_definitions(-DUNICODE -D_UNICODE)
    # Treat warnings as errors only for our code (not third-party)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4>)
endif()

include(FetchContent)

# Resources (SQL schema file) to be copied next to the executable
set(VSRM_RESOURCES
    resources/sql/schema.sql
    resources/images/logo.png
    resources/images/login_bg2.png
)

add_executable(vsrm WIN32
    src/win32/WinMain.cpp
    src/app/Database.cpp
    src/app/Database.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VSRM_RESOURCES})

# Copy resources to build output dir
foreach(res ${VSRM_RESOURCES})
    add_custom_command(TARGET vsrm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/${res}
        $<TARGET_FILE_DIR:vsrm>/$<IF:$<BOOL:$<STREQUAL:${res},resources/sql/schema.sql>>,schema.sql,$<IF:$<BOOL:$<STREQUAL:${res},resources/images/logo.png>>,logo.png,login_bg2.png>>)
endforeach()

# Platform libraries
if(WIN32)
    target_link_libraries(vsrm PRIVATE
        user32
        gdi32
        comctl32
        comdlg32
        shell32
        gdiplus
    )
endif()

# Find dependencies
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Link dependencies
target_link_libraries(vsrm PRIVATE
    unofficial::sqlite3::sqlite3
    imgui::imgui
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Enable precompiled headers
target_precompile_headers(vsrm PRIVATE
    <vector>
    <string>
    <memory>
    <filesystem>
    <windows.h>
    <gdiplus.h>
    <CommCtrl.h>
)

# Set Windows subsystem
set_target_properties(vsrm PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add compile definitions
target_compile_definitions(vsrm PRIVATE
    UNICODE
    _UNICODE
    VSRM_HAS_SQLITE3
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# Use Windows CNG (bcrypt) for SHA-256; no extra dependency
if(WIN32)
    target_link_libraries(vsrm PRIVATE bcrypt)
endif()

target_include_directories(vsrm PRIVATE src)

# Installation setup
include(GNUInstallDirs)
install(TARGETS vsrm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${VSRM_RESOURCES} DESTINATION ${CMAKE_INSTALL_BINDIR})


