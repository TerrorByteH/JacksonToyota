# VSRM (Vehicle Service Records Manager) — Toyota Zambia

VSRM is a Windows desktop application written in modern C++ (C++20) with a native Win32 GUI and SQLite storage. It digitizes Toyota Zambia's vehicle service records to improve efficiency, traceability, and customer satisfaction.

This repository includes:
- CMake build system (single solution for MSVC)
- vcpkg manifest for dependencies (SQLite)
- Minimal Win32 GUI showcasing core flows
- SQLite-backed data layer and schema
- Documentation for architecture, setup, and usage

## Features (v0.1)
- Vehicle service history management (persist records by VIN)
- Quick demo actions: insert a sample record, query by VIN
- Local SQLite database created on first run

### New in this iteration
- Local-only mechanics roster
- Local-only appointments and job assignments

## Limitations (MVP)
- Single-machine usage (no multi-user or remote access)
- No online booking/customer portal
- No parts inventory integration
- No third-party integrations

## Project Structure
```
.
├── CMakeLists.txt                # Build configuration
├── vcpkg.json                    # Dependencies (manifest mode)
├── src/
│   ├── app/
│   │   ├── Database.h            # DB interface and types
│   │   └── Database.cpp          # DB implementation (SQLite)
│   └── win32/
│       └── WinMain.cpp           # Win32 GUI entry point
├── resources/
│   └── sql/
│       └── schema.sql            # SQL schema + indices
└── README.md                     # You are here
```

## Prerequisites
- Windows 10 or later
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.21+ (bundled with VS 2022)
- vcpkg (manifest mode via VS integration or standalone)

If you don't have vcpkg integrated, install it once:
1. Clone vcpkg: `git clone https://github.com/microsoft/vcpkg` and run `bootstrap-vcpkg.bat`
2. Optionally integrate: `vcpkg integrate install`

## Build (VS 2022 GUI)
1. Open Visual Studio → File → Open → CMake...
2. Select the repository folder.
3. VS will detect `vcpkg.json` and install `sqlite3` automatically.
4. Set Configure Preset to x64 and Build.
5. Select the `vsrm` target and Run.

## Build (Command Line, x64)
```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

The resulting executable and `schema.sql` will be in `build/Release/` (for MSVC multi-config).

## Run
Run `vsrm.exe`. On first run it creates `vsrm.db` next to the executable and applies the schema from `schema.sql`.

In the app menu:
- File → "Add Sample Record" inserts a demo record
- File → "Query Sample VIN" lists records for the sample VIN in the main window

## Configuration
- Database path: same directory as the executable (`vsrm.db`)
- Schema path: `schema.sql` copied next to the executable at build time

## Architecture Overview
See `docs/ARCHITECTURE.md` for an in-depth explanation of layers and extension points, including future features (scheduling, assignments, analytics).

## Troubleshooting
- If SQLite isn't found: ensure you configured CMake with vcpkg toolchain and that `vcpkg.json` is present. Re-configure the project.
- If the window opens but DB errors appear: check that `schema.sql` exists next to `vsrm.exe` and isn't blocked by antivirus.

## License
Proprietary — Toyota Zambia internal use.


