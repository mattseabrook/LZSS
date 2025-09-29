#!/bin/bash

# build.sh – LZSS High-Performance Compression Suite
#
# Usage:
#   ./build.sh C23 windows     → C23 standard, Windows x64 EXE (MSVC runtime)
#   ./build.sh C23 linux       → C23 standard, Linux x64 ELF
#   ./build.sh CPP23 windows   → C++23 standard, Windows x64 EXE (MSVC runtime) 
#   ./build.sh CPP23 linux     → C++23 standard, Linux x64 ELF
#   ./build.sh ASM windows     → Assembly optimized, Windows x64 EXE
#   ./build.sh ASM linux       → Assembly optimized, Linux x64 ELF

set -e

# ------------------------------------------------------------------------------
# Configuration (shared)
# ------------------------------------------------------------------------------
PROJECT_ROOT=$(pwd)
BUILD_DIR="build"

# Build mode and platform configuration
setup_build_config() {
    BUILD_MODE="$1"
    PLATFORM="$2"
    
    case "$BUILD_MODE" in
        C23)
            SRC_FILE="lzss.c"
            COMPILER_FLAGS_BASE="-std=c2x -DC_MODE=1"
            ;;
        CPP23)
            SRC_FILE="lzss.cpp"  # Will create this
            COMPILER_FLAGS_BASE="-std=c++2b -DCPP_MODE=1"
            ;;
        ASM)
            SRC_FILE="lzss_asm.c"  # Will create this with inline asm
            COMPILER_FLAGS_BASE="-std=c2x -DASM_MODE=1 -masm=intel"
            ;;
        *)
            echo "ERROR: Unknown build mode '$BUILD_MODE'"
            echo "Supported modes: C23, CPP23, ASM"
            exit 1
            ;;
    esac
    
    case "$PLATFORM" in
        windows)
            TARGET_TRIPLE="x86_64-pc-windows-msvc"
            WINSDK_BASE="/opt/winsdk"
            EXE_NAME="lzss-${BUILD_MODE,,}.exe"
            ;;
        linux)
            TARGET_TRIPLE="x86_64-unknown-linux-gnu"
            EXE_NAME="lzss-${BUILD_MODE,,}"
            ;;
        *)
            echo "ERROR: Unknown platform '$PLATFORM'"
            echo "Supported platforms: windows, linux"
            exit 1
            ;;
    esac
    
    echo "Building LZSS ($BUILD_MODE mode) for $PLATFORM → $EXE_NAME"
}

# ------------------------------------------------------------------------------
# Windows-SDK helper
# ------------------------------------------------------------------------------
setup_winsdk() {
    echo "=== Setting up Windows SDK ==="

    if [[ ! -d "$WINSDK_BASE" ]]; then
        echo "Windows SDK not found → installing via xwin..."
        command -v xwin >/dev/null || { echo "Installing xwin..."; cargo install xwin; }
        sudo mkdir -p "$WINSDK_BASE"
        sudo chown "$USER":"$USER" "$WINSDK_BASE"
        xwin --accept-license splat --output "$WINSDK_BASE"
    fi

    # Detect layout produced by xwin
    DETECTED_SDK_INCLUDE="$WINSDK_BASE/sdk/include"
    DETECTED_SDK_LIB="$WINSDK_BASE/sdk/lib"
    DETECTED_CRT_INCLUDE="$WINSDK_BASE/crt/include"
    DETECTED_CRT_LIB="$WINSDK_BASE/crt/lib"
    DETECTED_LIB_ARCH="x86_64"

    # Pick the first SDK version directory we find
    for d in "$DETECTED_SDK_INCLUDE"/*/; do
        [[ -d "$d" ]] && { DETECTED_SDK_VERSION=$(basename "$d"); break; }
    done
    DETECTED_SDK_VERSION="${DETECTED_SDK_VERSION:-10.0.26100}"

    # Sanity check
    for p in "$DETECTED_SDK_INCLUDE/$DETECTED_SDK_VERSION/um" \
             "$DETECTED_SDK_INCLUDE/$DETECTED_SDK_VERSION/ucrt" \
             "$DETECTED_SDK_LIB/um/$DETECTED_LIB_ARCH" \
             "$DETECTED_CRT_LIB/$DETECTED_LIB_ARCH"; do
        [[ -d "$p" ]] || { echo "Missing path: $p"; exit 1; }
    done
    echo "Windows SDK ready."
}

# ------------------------------------------------------------------------------
# Source file generation for different modes
# ------------------------------------------------------------------------------
ensure_source_files() {
    # C++ version (stub for now)
    if [[ "$BUILD_MODE" == "CPP23" && ! -f "lzss.cpp" ]]; then
        echo "Creating C++23 stub: lzss.cpp"
        cat > lzss.cpp << 'EOF'
// lzss.cpp - C++23 LZSS implementation (STUB)
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::cout << "C++23 LZSS implementation - Coming soon!\n";
    std::cout << "Usage: " << argv[0] << " [e|d] input_file output_file\n";
    return EXIT_SUCCESS;
}
EOF
    fi
    
    # Assembly-optimized version (stub for now)
    if [[ "$BUILD_MODE" == "ASM" && ! -f "lzss_asm.c" ]]; then
        echo "Creating Assembly-optimized stub: lzss_asm.c"
        cat > lzss_asm.c << 'EOF'
// lzss_asm.c - Assembly-optimized LZSS implementation (STUB)
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Assembly-optimized LZSS implementation - Coming soon!\n");
    printf("Usage: %s [e|d] input_file output_file\n", argv[0]);
    return EXIT_SUCCESS;
}
EOF
    fi
}

# ------------------------------------------------------------------------------
# Build: Windows
# ------------------------------------------------------------------------------
build_windows() {
    echo "=== Building LZSS ($BUILD_MODE) for Windows x64 ==="
    setup_winsdk
    ensure_source_files
    
    mkdir -p "$BUILD_DIR"
    
    # Verify source file exists
    if [[ ! -f "$SRC_FILE" ]]; then
        echo "ERROR: Source file $SRC_FILE not found!"
        exit 1
    fi
    
    echo "Compiling $SRC_FILE → $EXE_NAME"
    
    # Compiler selection and flags based on build mode
    if [[ "$BUILD_MODE" == "CPP23" ]]; then
        COMPILER="clang++"
        LANG_FLAGS="/std:c++latest /Zc:__cplusplus /EHsc"
    else
        COMPILER="clang"  
        LANG_FLAGS="/std:c2x"
    fi
    
    # Compile directly to executable (single file project)
    set -x
    clang-cl --target="$TARGET_TRIPLE" \
        $LANG_FLAGS /MT /O3 /DNDEBUG /D_MT \
        "/imsvc$DETECTED_CRT_INCLUDE" \
        "/imsvc$DETECTED_SDK_INCLUDE/$DETECTED_SDK_VERSION/ucrt" \
        "/imsvc$DETECTED_SDK_INCLUDE/$DETECTED_SDK_VERSION/um" \
        "/imsvc$DETECTED_SDK_INCLUDE/$DETECTED_SDK_VERSION/shared" \
        $COMPILER_FLAGS_BASE \
        -o "$EXE_NAME" "$SRC_FILE" \
        /link \
        /subsystem:console \
        /defaultlib:libcmt \
        /defaultlib:libucrt \
        /nodefaultlib:msvcrt.lib \
        /libpath:"$DETECTED_SDK_LIB/um/$DETECTED_LIB_ARCH" \
        /libpath:"$DETECTED_SDK_LIB/ucrt/$DETECTED_LIB_ARCH" \
        /libpath:"$DETECTED_CRT_LIB/$DETECTED_LIB_ARCH" \
        kernel32.lib user32.lib
    set +x
    
    [[ -f "$EXE_NAME" ]] || { echo "ERROR: Build failed for $SRC_FILE"; exit 1; }
    echo "✅ Build complete → ./$EXE_NAME"
}

# ------------------------------------------------------------------------------
# Build: Linux
# ------------------------------------------------------------------------------
build_linux() {
    echo "=== Building LZSS ($BUILD_MODE) for Linux x64 ==="
    ensure_source_files
    
    mkdir -p "$BUILD_DIR"
    
    # Verify source file exists
    if [[ ! -f "$SRC_FILE" ]]; then
        echo "ERROR: Source file $SRC_FILE not found!"
        exit 1
    fi
    
    echo "Compiling $SRC_FILE → $EXE_NAME"
    
    # Compiler selection and flags based on build mode
    if [[ "$BUILD_MODE" == "CPP23" ]]; then
        COMPILER="clang++"
        LANG_FLAGS="-std=c++2b"
    else
        COMPILER="clang"
        LANG_FLAGS="-std=c2x"
    fi
    
    # Compile directly to executable (single file project)
    set -x
    $COMPILER $LANG_FLAGS -O3 -DNDEBUG -march=native \
        $COMPILER_FLAGS_BASE \
        -o "$EXE_NAME" "$SRC_FILE"
    set +x
    
    [[ -f "$EXE_NAME" ]] || { echo "ERROR: Build failed for $SRC_FILE"; exit 1; }
    echo "✅ Build complete → ./$EXE_NAME"
}

# ------------------------------------------------------------------------------
# Clean
# ------------------------------------------------------------------------------
clean() {
    echo "Cleaning build artifacts..."
    rm -rf "$BUILD_DIR" lzss-* *.pdb *.exe || true
    echo "✅ Clean complete"
}

# ------------------------------------------------------------------------------  
# Entry point
# ------------------------------------------------------------------------------
if [[ $# -eq 1 && "$1" == "clean" ]]; then
    clean
    exit 0
elif [[ $# -ne 2 ]]; then
    echo "LZSS High-Performance Compression Suite"
    echo ""
    echo "Usage: $0 <BUILD_MODE> <PLATFORM>"
    echo ""
    echo "BUILD_MODE:"
    echo "  C23      - C23 standard implementation (current)"
    echo "  CPP23    - C++23 implementation (stub)"
    echo "  ASM      - Assembly-optimized implementation (stub)"
    echo ""
    echo "PLATFORM:"
    echo "  windows  - Cross-compile Windows x64 EXE (MSVC runtime)"  
    echo "  linux    - Native Linux x64 ELF"
    echo ""
    echo "Examples:"
    echo "  ./build.sh C23 windows     # Build C23 version for Windows"
    echo "  ./build.sh C23 linux       # Build C23 version for Linux"
    echo "  ./build.sh CPP23 windows   # Build C++23 version for Windows" 
    echo "  ./build.sh ASM linux       # Build ASM version for Linux"
    echo "  ./build.sh clean           # Clean build artifacts"
    echo ""
    exit 1
fi

BUILD_MODE="$1"
PLATFORM="$2"

setup_build_config "$BUILD_MODE" "$PLATFORM"

case "$PLATFORM" in
    windows)
        build_windows
        ;;
    linux)
        build_linux
        ;;
    *)
        echo "ERROR: Unknown platform '$PLATFORM'"
        exit 1
        ;;
esac

echo ""
echo "🚀 Build Summary:"
echo "   Mode: $BUILD_MODE"
echo "   Platform: $PLATFORM"  
echo "   Output: $EXE_NAME"
echo "   Source: $SRC_FILE"
echo ""