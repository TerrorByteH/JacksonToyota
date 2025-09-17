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

# Detect if building with MSVC and set Unicode
if(MSVC)
    add_definitions(-DUNICODE -D_UNICODE)
    # Treat warnings as errors only for our code (not third-party)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4>)
endif()

# vcpkg manifest mode will pull dependencies declared in vcpkg.json
# We depend on sqlite3 via vcpkg

include(FetchContent)

# Resources (SQL schema file) to be copied next to the executable
set(VSRM_RESOURCES
    resources/sql/schema.sql
    resources/images/logo.png
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
        $<TARGET_FILE_DIR:vsrm>/schema.sql)
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

# Find SQLite3 from vcpkg (manifest mode) or system
find_package(unofficial-sqlite3 CONFIG QUIET)
if(unofficial-sqlite3_FOUND)
    target_link_libraries(vsrm PRIVATE unofficial::sqlite3::sqlite3)
    target_compile_definitions(vsrm PRIVATE VSRM_HAS_SQLITE3)
else()
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        target_link_libraries(vsrm PRIVATE SQLite::SQLite3)
        target_compile_definitions(vsrm PRIVATE VSRM_HAS_SQLITE3)
    else()
        message(WARNING "SQLite3 not found. The app will build, but DB features will be disabled.")
    endif()
endif()

target_include_directories(vsrm PRIVATE src)

# Installation setup
include(GNUInstallDirs)
install(TARGETS vsrm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${VSRM_RESOURCES} DESTINATION ${CMAKE_INSTALL_BINDIR})


