**Table-of-Contents**
- [Introduction](#introduction)
- [2022](#2022)
  - [Stand-alone](#stand-alone)
    - [Use Case](#use-case)
  - [Header](#header)
  - [Library](#library)
- [Developer Notes](#developer-notes)
  - [Testing](#testing)
- [1989](#1989)
  - [LZSS coding](#lzss-coding)
    - [References](#references)
  - [License](#license)

# Introduction

Lempel–Ziv–Storer–Szymanski (LZSS) is a Dictionary-type lossless data compression algorithm that was created in 1982. For more information, see the [Wikipedia article](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Storer%E2%80%93Szymanski) on LZSS or the [1989](#1989) section in this `README`.

# 2022

2022 Refactoring of the 1989 LZSS.C public domain code written by Haruhiko Okumura. My goal is to give users a choice of using the code as a stand-alone program, as a header file, or as a library. 

## Stand-alone

The stand-alone program is a binary executable for compressing and decompressing files from a command-line interface.

### Use Case

Binary packing for a custom game engine. The game engine itself can have the LZSS library statically linked to perform decompression in real-time, but at some point during the build process of the engine itself there would be a requirement to have an executable that can compress files. This way, each individual asset (eg Bitmap, WAV, MIDI, etc) can be compressed at build time. This would allow the game engine to load assets from disk without having to decompress them at runtime, but that is not really a thing to be concerned about in 2022. It's more about adding another layer of "copy protection" to the individual game assets that would be contained within a larger archive file shipped with the final version.

## Header

x

## Library 

x

# Developer Notes

x

## Testing

x

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