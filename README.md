**Table-of-Contents**
- [Introduction](#introduction)
  - [2026](#2026)
    - [Differences Between Original 1989 `LZSS.C` and 2026 Updated Version](#differences-between-original-1989-lzssc-and-2026-updated-version)
      - [Architecture](#architecture)
      - [Performance](#performance)
      - [Code Quality](#code-quality)
      - [Build System](#build-system)
- [Build](#build)
- [Developers](#developers)
  - [References](#references)
- [1989](#1989)
  - [LZSS coding](#lzss-coding)
    - [References](#references-1)
  - [License](#license)
- [CHANGELOG](#changelog)
  - [2025-12-12](#2025-12-12)
  - [2024-12-08](#2024-12-08)
  - [2023-10-24](#2023-10-24)
  - [2022-11-24](#2022-11-24)
  - [2022-11-21](#2022-11-21)
  - [2022-11-18](#2022-11-18)

# Introduction

Lempel–Ziv–Storer–Szymanski (LZSS) is a Dictionary-type lossless data compression algorithm that was created in 1982. For more information, see the [Wikipedia article](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Storer%E2%80%93Szymanski) on LZSS or the [1989](#1989) section in this `README`.

## 2026

Complete rewrite of the original 1989 LZSS.c public domain code written by Haruhiko Okumura. Rebuilt from the ground up for C23 with zero-copy I/O, cross-platform native compilation, and 7th Guest / retro game format compatibility.

### Differences Between Original 1989 `LZSS.C` and 2026 Updated Version

#### Architecture
- **Zero-Copy Memory-Mapped I/O**
  - Input files are memory-mapped directly (`mmap` on POSIX, `MapViewOfFile` on Windows)
  - Eliminates per-byte `fgetc`/`fputc` overhead entirely
  - Sequential access hints (`MADV_SEQUENTIAL`) for optimal kernel prefetching

- **Unified Cross-Platform Design**
  - Single source file compiles natively on Linux and Windows
  - Platform-specific code isolated via `#ifdef _WIN32` blocks
  - No runtime dependencies or emulation layers

- **Original Parameters Preserved**
  - `N = 4096` (ring buffer size)
  - `F = 18` (maximum match length)
  - `THRESHOLD = 2` (minimum match for encoding)
  - Fully compatible with 7th Guest, 11th Hour, and other retro game LZSS formats

#### Performance
- **Binary Search Tree Matching**
  - O(log N) average-case match finding using 256 separate BSTs (one per first byte)
  - Original Okumura algorithm preserved for correctness and compatibility
  
- **Optimized Memory Layout**
  - Compact `State` struct (~45KB) contains ring buffer and all tree arrays
  - Single allocation per encode/decode operation
  - Output buffer with geometric growth (1.5x) to minimize reallocations

- **Progress Reporting**
  - Real-time encoding progress displayed every 1MB
  - Compression ratio reported on completion

#### Code Quality
- **C23 Compliance**
  - `_Static_assert` for compile-time parameter validation
  - Modern enum declarations with explicit types
  - Clean separation of concerns

- **Minimal Footprint**
  - ~220 lines of code (down from 400+ in previous versions)
  - No external dependencies beyond libc
  - Single source file, no headers required

- **Binary-Safe**
  - Handles all 256 byte values correctly
  - No ASCII assumptions anywhere in the codebase

#### Build System
- **Cross-Compilation Support**
  - Native Windows EXE from Linux using `clang-cl` + Windows SDK via `xwin`
  - No Wine, no MSYS2, no emulation—produces genuine PE/COFF executables
  - MSVC ABI compatible (static CRT linking)

- **Visual Build Output**
  - Color-coded terminal output with emoji status indicators
  - Build timing and binary size reporting
  - One command: `./build.sh linux` or `./build.sh windows`

# Build

Cross-platform build system using Clang. Produces native binaries for both Linux and Windows from a single Linux host.

```bash
# Show help
./build.sh

# Build for Linux (native ELF)
./build.sh linux

# Build for Windows (native PE/COFF, no emulation)
./build.sh windows

# Build both platforms
./build.sh all

# Clean build artifacts
./build.sh clean
```

**Requirements:**
- `clang` (C23 support)
- `clang-cl` and `lld-link` (for Windows cross-compilation)
- Windows SDK via [`xwin`](https://github.com/Jake-Shadle/xwin) (auto-installed on first Windows build)

**Output:**
- `./lzss` — Linux x64 ELF binary
- `./lzss.exe` — Windows x64 PE binary (runs natively on Windows, no Wine needed)

# Developers

## References

- [7th Guest VDX](http://wiki.xentax.com/index.php/The_7th_Guest_VDX)
- [Retro Modding Wiki: LZSS Compression](https://wiki.axiodl.com/w/LZSS_Compression)
- [XeNTaX: LZSS](http://wiki.xentax.com/index.php/LZSS)
  
# 1989

The original code is written in C and is compatible with C89 in the context of Linux. Only the information pertaining to LZSS from Haruhiko Okumura's 1989 `COMPRESS.TXT` file has been surfaced in this `README`.

## LZSS coding

This scheme is initiated by Ziv and Lempel [1].  A slightly modified
version is described by Storer and Szymanski [2].  An implementation
using a binary tree is proposed by Bell [3].  The algorithm is quite
simple: Keep a ring buffer, which initially contains "space" characters
only.  Read several letters from the file to the buffer.  Then search
the buffer for the longest string that matches the letters just read,
and send its length and position in the buffer. 

If the buffer size is 4096 bytes, the position can be encoded in 12
bits.  If we represent the match length in four bits, the <position,
length> pair is two bytes long.  If the longest match is no more than
two characters, then we send just one character without encoding, and
restart the process with the next letter.  We must send one extra bit
each time to tell the decoder whether we are sending a <position,
length> pair or an unencoded character. 

The accompanying file LZSS.C is a version of this algorithm.  This
implementation uses multiple binary trees to speed up the search for the
longest match.  All the programs in this article are written in
draft-proposed ANSI C.  I tested them with Turbo C 2.0. 

### References

- [1] J. Ziv and A. Lempel, IEEE Trans. IT-23, 337-343 (1977).
- [2] J. A. Storer and T. G. Szymanski, J. ACM, 29, 928-951 (1982).
- [3] T. C. Bell, IEEE Trans. COM-34, 1176-1182 (1986).

## License

`LZHUF.C` is Copyrighted, the rest is Public Domain.

# CHANGELOG

## 2025-12-12

- **Complete rewrite** of LZSS implementation in C23
- Zero-copy memory-mapped I/O (`mmap`/`MapViewOfFile`)
- Binary search tree matching (O(log N) average case)
- Cross-platform build system with native Windows EXE output via `clang-cl` + `xwin`
- Reduced codebase from 400+ lines to ~220 lines
- Fixed critical bug: `tree_init` was using `memset` with `NIL & 0xFF` (=0) instead of proper 16-bit NIL initialization
- Fixed pointer type mismatch in `insert()` function
- Added real-time progress indicator during encoding
- Preserved original 1989 Okumura parameters (N=4096, F=18, THRESHOLD=2) for 7th Guest compatibility
- Removed legacy C++17 version and Visual Studio solution

## 2024-12-08

- `C23` version created, compiled with no warnings using `clang -std=c23 -Wall -O3 -g lzss.c -o lzss.exe`, but there are definitely issues to debug/troubleshoot. 
- Documentation updates.

## 2023-10-24

- Visual Studio 2022 C++ Solution and refactored `lzss.cpp` using latest AI models was added! (legacy, DO NOT USE, included & archived for future analysis)

## 2022-11-24

- Used `lldb` to figure out why the new C++ version couldn't break out of the `for(;;)` loop in `InsertNode`. Turns out that the `r` value was one byte larger than the `C89` version in a side-by-side comparison.
- Somehow, even using co-pilot it missed the `InitTree` call inside the `Encode` function. This was causing the `r` value to be off by one byte.
- Documentation updates.

## 2022-11-21

- Added my own style of commenting and layout to the code
- Added the `main` function / Entry Point to the code
- Added a `helpText` function to display the help text to the command-line
- Added the ASCII Art logo to the code
- Added error checking in the `main` function for the input and output files for the Stand-alone solution.

## 2022-11-18

- Moved from `C89` to `C++17`
- Changed `define` to `const` where applicable
- Changed `FILE` to `fstream` where applicable
- Changed most variable names to camelCase and full english words
- Replaced the various `printf` calls with a mix of `std::cout`, `std::fixed`, and `std::precision` as needed.
