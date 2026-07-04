#!/bin/sh

echo "Starting build process for gitool..."

if [ $# -gt 0 ] && [ "$1" = "clean" ]; then
    echo "Cleaning up old build files..."
    make clean
    exit 0
fi

if [ $# -gt 0 ] && [ "$1" = "rebuild" ]; then
    echo "Rebuilding project from scratch..."
    make clean
fi

echo "Compiling source code..."
if make -j"$(nproc 2>/dev/null || echo 1)"; then
    echo "Build successful! Located at: build/gitool"
    echo "You can now run 'sudo make install' to deploy globally."
    sudo make install
else
    echo "Build failed. Please check the compiler warnings/errors above."
    exit 1
fi
