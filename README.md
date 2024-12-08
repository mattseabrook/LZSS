**Table-of-Contents**
- [Introduction](#introduction)
  - [2024](#2024)
    - [Differences Between Original 1989 `LZSS.C` and 2024 Updated Version](#differences-between-original-1989-lzssc-and-2024-updated-version)
      - [General Changes](#general-changes)
      - [Variable and Structure Updates](#variable-and-structure-updates)
      - [Functional Enhancements](#functional-enhancements)
      - [File Processing](#file-processing)
      - [Code Structure and Organization](#code-structure-and-organization)
      - [Debugging and Output](#debugging-and-output)
      - [Additional Features](#additional-features)
      - [Removed Legacy Components](#removed-legacy-components)
- [Build](#build)
  - [Linux](#linux)
  - [Windows](#windows)
- [Usage](#usage)
  - [Linux](#linux-1)
  - [Windows](#windows-1)
- [Developers](#developers)
  - [Inspecting the generated LZS files](#inspecting-the-generated-lzs-files)
  - [References](#references)
- [1989](#1989)
  - [LZSS coding](#lzss-coding)
    - [References](#references-1)
  - [License](#license)
- [CHANGELOG](#changelog)
  - [2024-12-08](#2024-12-08)
  - [2023-10-24](#2023-10-24)
  - [2022-11-24](#2022-11-24)
  - [2022-11-21](#2022-11-21)
  - [2022-11-18](#2022-11-18)
- [TODO](#todo)
- [Notes](#notes)
  - [Encode](#encode)

# Introduction

Lempel–Ziv–Storer–Szymanski (LZSS) is a Dictionary-type lossless data compression algorithm that was created in 1982. For more information, see the [Wikipedia article](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Storer%E2%80%93Szymanski) on LZSS or the [1989](#1989) section in this `README`.

## 2024 

2024 Refactoring of the original 1989 LZSS.c public domain code written by Haruhiko Okumura. Updated for the `C23` specification, here is a complete list of the enhancements made:

### Differences Between Original 1989 `LZSS.C` and 2024 Updated Version

#### General Changes
- **Language Modernization**
  - Refactored to use C23-compliant practices and modern C idioms.
  - Enabled `_CRT_SECURE_NO_WARNINGS` for compatibility with modern compilers.

- **Memory Safety and Error Handling**
  - Introduced `safe_fopen` function for secure file handling with detailed error messages.
  - Improved memory allocation checks for critical resources (`calloc`, `malloc`).

#### Variable and Structure Updates
- **Parameter Definitions**
  - `N` -> `RING_BUFFER_SIZE`: Descriptive name for the circular buffer size.
  - `F` -> `MATCH_MAX_LEN`: Maximum match length.
  - `THRESHOLD` -> `MATCH_THRESHOLD`: Minimum match length for encoding.
  - `NIL` -> `NODE_UNUSED`: Symbol for unused binary tree nodes.

- **Binary Tree Nodes**
  - Replaced separate arrays `lson`, `rson`, and `dad` with a `TreeNode` struct to encapsulate left, right, and parent pointers.

#### Functional Enhancements
- **Tree Management**
  - `initialize_tree`: Updated for `TreeNode` structure, improved readability.
  - `insert_node`: Modernized logic to use new data structures and avoid unnecessary complexity.

- **Encoding Enhancements**
  - Simplified buffer initialization with `memset`.
  - Updated match handling logic to improve clarity.
  - Rewrote loops for explicit `EOF` handling, improving robustness.
  - Added detailed error handling for memory allocation failures.

- **Decoding Enhancements**
  - Streamlined decoding process with consistent `EOF` checks.
  - Improved flag handling for clarity and performance.
  - Used descriptive variable names for better readability.

#### File Processing
- **File I/O**
  - Replaced manual `fopen` calls with `safe_fopen`, ensuring proper error reporting.
  - Ensured all file handles are securely closed after use.

#### Code Structure and Organization
- **Refactored for Modularity**
  - Separated tree initialization, insertion, encoding, and decoding into distinct functions.
  - Reduced redundancy and improved maintainability.

- **Memory Management**
  - Encapsulated dynamic memory allocation in encoding and decoding processes.
  - Added explicit cleanup steps to prevent memory leaks.

#### Debugging and Output
- **Progress and Error Messages**
  - Added verbose error messages for file and memory operations.
  - Output now includes detailed information on encoding and decoding success.

#### Additional Features
- **Boolean Logic**
  - Introduced `bool` type and variables (`<stdbool.h>`) for better logical operations.
  - Improved clarity in loops and conditional checks.

- **Switch-Based Main Logic**
  - Simplified main entry point using `switch` for encoding/decoding selection.
  - Improved argument validation with descriptive error messages.

- **Comprehensive Resource Cleanup**
  - Ensured all dynamically allocated resources are freed properly, even in error cases.
  - Added checks to prevent double-free errors.

#### Removed Legacy Components
- **Progress Reporting**
  - Removed periodic progress printing in favor of simplified operation reporting.
  - Replaced old `printf` debug statements with consistent error and status messages.

# Build

## Linux

```bash
# Build with clang

clang -std=c23 -Weverything -O3 -g lzss.c -o lzss
```

## Windows

```cmd
clang -std=c23 -Weverything -O3 -g lzss.c -o lzss.exe
```

# Usage

Included `sample.ppm` as test binary data.

## Linux

```bash
# Compress
lzss e sample.ppm sample.lzs

# Decompress
lzss d sample.lzs sample.ppm
```

## Windows

x

# Developers

## Inspecting the generated LZS files

```bash
# file analysis of sample.ppm
file sample.ppm
```

The output should exactly match: `sample.ppm: Netpbm image data, size = 1920 x 1280, rawbits, pixmap`

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

## 2024-12-08

- `C23` version created, compiled with no warnings using `clang -std=c23 -Wall -O3 -g lzss.c -o lzss.exe`, but there are definitely issues to debug/troubleshoot. 

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

# TODO

- Test Windows compile
- After testing see if `nil` can be removed from the 2022 code
- Remove all of the documentation from the 2022 code and put it in the `README`
- Expose `N`, `F`, `THRESHOLD`, and possibly others. This will be different for the Stand-alone, Header, and Library solutions.

# Notes

## Encode

- The original algorithm employs a complex scheme of storing the position and length. We're simplifying it by directly storing these values into the buffer. This may need further adjustments based on the decoding scheme.