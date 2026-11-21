**Table of contents**

- [Input cursor width](#input-cursor-width)
- [Assembly](#assembly)
  - [Build](#build)
- [C23](#c23)
  - [Header-only implementation](#header-only-implementation)
  - [Executable](#executable)
- [Usage](#usage)
- [Developers](#developers)
  - [Toolchains](#toolchains)
- [References](#references)
  - [Links](#links)
  - [1989](#1989)
  - [LZSS coding](#lzss-coding)
  - [License](#license)

Lempel–Ziv–Storer–Szymanski (LZSS) is a dictionary-based lossless compression algorithm created in 1982. This repository provides a standalone C23 header and command-line executables with C23 or assembly codecs. Both use Haruhiko Okumura's 4 KiB sliding window, 18-byte maximum match and least-significant-bit-first flags for the raw LZSS format used by The 7th Guest. [Original 1989 notes](#1989) · [ISO/IEC 9899:2024](https://www.iso.org/standard/82075.html)

Both executables memory-map input files. The encoder uses roughly 29 KiB of scratch space, and the decoder uses bytes already produced as its history. The output buffer grows as needed. The CLI opens the destination after processing the input, so the same path can serve as input and output. Peak memory depends on output size.

# Input cursor width

`LZSS_CURSOR_BITS` sets the input size limit for both the C and assembly executables. Both build scripts default to `64` and accept a per-build override. Run these examples from the repository root.

```bash
./C/build.sh linux                       # C, default 64-bit input limit
./ASM/build.sh linux                     # Assembly, default 64-bit input limit
LZSS_CURSOR_BITS=32 ./C/build.sh linux    # C, 32-bit input limit
LZSS_CURSOR_BITS=32 ./ASM/build.sh linux  # Assembly, 32-bit input limit
```

| Value | Input size for both executables |
| ----- | ------------------------------- |
| `64` | Supports inputs of 4 GiB and larger when the OS, address space and available memory permit. |
| `32` | Caps inputs at 4 GiB minus one byte (4,294,967,295 bytes). |

The shared CLI enforces the limit before mapping the input file. The C codec uses 32-bit input cursors in `32` mode. The assembly codecs use 64-bit input cursors in both modes. Both settings use the same executable architecture and raw LZSS format.

For header-only C use, compile with `-DLZSS_CURSOR_BITS=32` to select the same 32-bit input limit.

# Assembly

The x86-64 codec is written in NASM and assembles to ELF, Win64 COFF, or Mach-O. The AArch64 codec uses Clang's assembler for native Apple Silicon and Linux Arm64. Both implement the match tree, encoder and decoder in assembly, using the C codec's match order and tie-breaking rules to produce the same compressed bytes. They share the C front end for file I/O, buffer allocation and CLI messages. [NASM documentation](https://www.nasm.us/doc/nasm01.html)

## Build

Run these commands from ASM directory.

| Command                  | Output                                          |
| ------------------------ | ----------------------------------------------- |
| `./ASM/build.sh linux`   | `ASM/lzss`, x86-64 or AArch64 ELF               |
| `./ASM/build.sh windows` | `ASM/lzss.exe`, native Windows x64 PE/COFF      |
| `./ASM/build.sh macos`   | `ASM/lzss-macos`, Intel or Apple Silicon Mach-O |
| `./ASM/build.sh all`     | All three targets in parallel                   |
| `./ASM/build.sh clean`   | Removes the assembly executables                |

Set `LZSS_OUTPUT_DIR` to override the output directory. Install NASM for x86-64 targets and use a Clang-based toolchain for AArch64. See [toolchains](#toolchains) for platform setup.

# C23

## Header-only implementation

Copy [`C/lzss.h`](C/lzss.h) into your project and include it from C23. The header contains the whole in-memory codec and uses only the C standard library. It needs no companion object, platform headers or implementation macro. You can include it in multiple translation units.

```c
#include "lzss.h"

int main(void) {
  const unsigned char input[] = {0, 1, 2, 0, 1, 2};
  lzss_buffer packed = {0};
  lzss_buffer restored = {0};

  if (!lzss_encode(input, sizeof input, &packed))
    return 1;

  bool ok = lzss_decode(packed.data, packed.size, &restored);
  lzss_free(&restored);
  lzss_free(&packed);
  return ok ? 0 : 1;
}
```

Pass a fresh or released `lzss_buffer` to `lzss_encode` or `lzss_decode`. On success, `data` and `size` describe the result. On failure the result is empty. Release it with `lzss_free` before reusing it. Empty input is valid, including `NULL` with a size of zero. The header uses standard allocation and needs enough memory for the output and encoder workspace. For a freestanding target, supply those library facilities and budget the workspace before integrating it.

## Executable

Run these commands from the C directory.

| Command                | Output                            |
| ---------------------- | --------------------------------- |
| `./C/build.sh linux`   | `C/lzss`, native Linux ELF        |
| `./C/build.sh windows` | `C/lzss.exe`, Windows x64 PE/COFF |
| `./C/build.sh macos`   | `C/lzss-macos`, macOS Mach-O      |
| `./C/build.sh all`     | All three targets in parallel     |
| `./C/build.sh clean`   | Removes the C executables         |

Set `LZSS_OUTPUT_DIR` to override the output directory.

# Usage

Both executables use the same CLI. These examples use the Linux binary names.

```bash
# Compress a file
./C/lzss e input.bin output.lzs

# Decompress the file with the assembly executable
./ASM/lzss d output.lzs restored.bin
```

# Developers

## Toolchains

On Ubuntu 24.04, install the versioned LLVM packages and expose the tools on your `PATH`. On Arch Linux, use the distribution packages. The `nasm` package is needed for x86-64 assembly builds.

```bash
# Ubuntu 24.04
sudo apt install build-essential clang-19 lld-19 nasm
export PATH="/usr/lib/llvm-19/bin:$PATH"

# Arch Linux
sudo pacman -S --needed clang lld nasm
```

- **Linux** defaults to the native Clang target. Set `CC_LINUX` to a configured cross-compiler and `LINUX_SYSROOT` if needed.
- **Windows** uses `clang-cl`, `lld-link`, and the Microsoft SDK and static CRT to link a native x64 executable from Linux without Wine or emulation. The script checks `WINSDK_BASE`, `/opt/winsdk`, the v64tng cache and the LZSS cache. If the SDK is absent, add `xwin` to your `PATH` and rerun the build to download it and review the SDK license. On Windows, use a Visual Studio developer environment with LLVM available in Git Bash, MSYS2, or Cygwin. [xwin releases](https://github.com/Jake-Shadle/xwin/releases)
- **macOS** uses a current Xcode Command Line Tools installation on the Mac. The native build defaults to the host architecture. Use `MACOS_ARCH=arm64` or `MACOS_ARCH=x86_64` to choose a target. On Linux, use osxcross, a configured `CC_MACOS`, or an Apple SDK in `SDKROOT` with Clang and `ld64.lld`. The SDK-based cross-build defaults to Intel and macOS 14.0. Use `MACOS_ARCH=arm64` for Apple Silicon and `MACOSX_DEPLOYMENT_TARGET` to set the minimum OS version. Install NASM for the Intel assembly path. [osxcross](https://github.com/tpoechtrager/osxcross)

# References

## Links

- [7th Guest VDX](http://wiki.xentax.com/index.php/The_7th_Guest_VDX)
- [Retro Modding Wiki: LZSS Compression](https://wiki.axiodl.com/w/LZSS_Compression)
- [XeNTaX: LZSS](http://wiki.xentax.com/index.php/LZSS)
  
## 1989

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

- [1] J. Ziv and A. Lempel, IEEE Trans. IT-23, 337-343 (1977).
- [2] J. A. Storer and T. G. Szymanski, J. ACM, 29, 928-951 (1982).
- [3] T. C. Bell, IEEE Trans. COM-34, 1176-1182 (1986).

## License

`LZHUF.C` is Copyrighted, the rest is Public Domain.
