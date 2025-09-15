#!/bin/bash

# ArkLights Build Script
echo "Building ArkLights PEV Lighting System..."

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    echo "PlatformIO not found. Please install PlatformIO first."
    echo "Run: pip install platformio"
    exit 1
fi

# Build the project
echo "Building for ESP32-S3..."
pio run -e arklights_esp32s3

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Firmware size:"
    pio run -e arklights_esp32s3 -t size
else
    echo "Build failed. Check the errors above."
    exit 1
fi
