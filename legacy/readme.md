# Legacy WLED Files

This folder contains all the original WLED files and configurations that were part of the base WLED project. These files are preserved for reference but are not used in the current ArkLights PEV implementation.

## Contents

- **wled00/** - Original WLED source code
- **usermods/** - WLED user modifications and extensions
- **tools/** - WLED-specific tools and utilities
- **pio-scripts/** - PlatformIO build scripts for WLED
- **images/** - WLED logos and images
- **lib/** - WLED libraries
- **include/** - WLED header files
- **test/** - WLED test files
- **variants/** - WLED board variants
- **boards/** - WLED board configurations
- **build_output/** - Previous build outputs
- **node_modules/** - Node.js dependencies for WLED web interface
- **package.json** - Node.js package configuration
- **platformio.ini** - Original WLED PlatformIO configuration
- **CMakeLists.txt** - WLED CMake configuration
- **build.sh** - WLED build script
- Various documentation files (CHANGELOG.md, CONTRIBUTING.md, etc.)

## Current ArkLights Implementation

The current ArkLights PEV lighting system uses only:
- `src/main.cpp` - Custom ArkLights implementation
- `platformio_simple.ini` - Simplified PlatformIO configuration
- `platformio_arklights.ini` - ArkLights-specific configuration
- `ARKLIGHTS_README.md` - ArkLights documentation

All WLED-specific functionality has been replaced with a custom, focused implementation designed specifically for PEV lighting applications.