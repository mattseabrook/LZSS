**Table-of-Contents**
- [Introduction](#introduction)
- [2022](#2022)
  - [Stand-alone](#stand-alone)
    - [Use Case](#use-case)
      - [Binary packing for a custom game engine](#binary-packing-for-a-custom-game-engine)
      - [Server-side compression](#server-side-compression)
    - [Usage](#usage)
      - [Linux](#linux)
      - [Windows](#windows)
  - [Header](#header)
  - [Library](#library)
    - [Use Case](#use-case-1)
      - [Static Linking to a custom Game Engine or Tool](#static-linking-to-a-custom-game-engine-or-tool)
    - [Usage](#usage-1)
      - [Linux](#linux-1)
      - [Windows](#windows-1)
- [Developer Notes](#developer-notes)
  - [Testing](#testing)
  - [References](#references)
  - [Documentation](#documentation)
- [1989](#1989)
  - [LZSS coding](#lzss-coding)
    - [References](#references-1)
  - [License](#license)
- [CHANGELOG](#changelog)
  - [2022-11-21](#2022-11-21)
  - [2022-11-18](#2022-11-18)
- [TODO](#todo)

# Introduction

Lempel–Ziv–Storer–Szymanski (LZSS) is a Dictionary-type lossless data compression algorithm that was created in 1982. For more information, see the [Wikipedia article](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Storer%E2%80%93Szymanski) on LZSS or the [1989](#1989) section in this `README`.

# 2022

2022 Refactoring of the 1989 LZSS.C public domain code written by Haruhiko Okumura. My goal is to give users a choice of using the code as a stand-alone program, as a header file, or as a library. 

## Stand-alone

The stand-alone program is a binary executable for compressing and decompressing files from a command-line interface.

### Use Case

#### Binary packing for a custom game engine 

The game engine itself can have the LZSS library statically linked to perform decompression in real-time, but at some point during the build process of the engine itself there would be a requirement to have an executable that can compress files. This way, each individual asset (eg Bitmap, WAV, MIDI, etc) can be compressed at build time. This would allow the game engine to load assets from disk without having to decompress them at runtime, but that is not really a thing to be concerned about in 2022. It's more about adding another layer of "copy protection" to the individual game assets that would be contained within a larger archive file shipped with the final version.

#### Server-side compression

The stand-alone program can be used with `sqlite3`, as an example, to compress some data before storing it in a database, that would be used on the back-end of a web application, etc.

### Usage

#### Linux

```bash
# Build with clang
clang++ -O3 -o lzss lzss.cpp

# First time run
./lzss2022
```

Output:

```text
  _     _________ ____
 | |   |__  / ___/ ___|
 | |     / /\___ \___ \
 | |___ / /_ ___) |__) |
 |_____/____|____/____/

Lempel-Ziv-Storer-Szymanski (LZSS) compression algorithm
11/21/2022 by Matt Seabrook

        Usage: lzss [-e|-d] infile outfile
        
```

#### Windows

x

## Header

x

## Library 

x

### Use Case

#### Static Linking to a custom Game Engine or Tool

A core function of this custom Game Engine would be to decompress assets in real-time. The game engine would have the LZSS library statically linked to it in this case.

### Usage

#### Linux

```bash
x
```

#### Windows

x

# Developer Notes

There were no issues compiling the original LZSS.C code with `clang` on Linux.

```bash
# Compile the original 1989 LZSS.C code to test against
clang -O3 -o lzss1989 lzss.c
```

## Testing

x

## References

  [7th Guest VDX](http://wiki.xentax.com/index.php/The_7th_Guest_VDX)
- [Retro Modding Wiki: LZSS Compression](https://wiki.axiodl.com/w/LZSS_Compression)
  
## Documentation

Automatically produce flow control diagrams of the *.cpp source files

```bash
clang -S -emit-llvm -o hello.ll hello.cpp
opt hello.ll -dot-cfg -o hello.dot
```

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