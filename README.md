**Table-of-Contents**
- [Introduction](#introduction)
- [2022](#2022)
  - [Stand-alone](#stand-alone)
  - [Header](#header)
  - [Library](#library)
- [1989](#1989)
  - [LZSS coding](#lzss-coding)
    - [References](#references)
  - [License](#license)

# Introduction

2022 Refactoring of the 1989 LZSS.C public domain code written by Haruhiko Okumura

# 2022

x

## Stand-alone

x

## Header

x

## Library 

x

# 1989

Only the LZSS information from Haruhiko Okumura's 1989 `COMPRESS.TXT` file has been surfaced here.

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