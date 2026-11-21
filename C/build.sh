#!/usr/bin/env bash
set -euo pipefail
[[ ${LZSS_DEBUG:-0} != 1 ]] || set -x

# Use 32 to cap inputs at 4 GiB minus one byte and use narrower cursors.
LZSS_CURSOR_BITS=${LZSS_CURSOR_BITS:-64}
root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
out=${LZSS_OUTPUT_DIR:-$root}
implementation=${LZSS_IMPLEMENTATION:-c}
asm_dir="$root/../ASM"
flags=(-std=c23 -O3 -DNDEBUG -Wall -Wextra -Wpedantic "-DLZSS_CURSOR_BITS=$LZSS_CURSOR_BITS")
backend=()
if [[ $implementation == asm ]]; then backend=(-DLZSS_USE_ASM); fi

die() { printf '%s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null || die "Missing $1. $2"; }
usage() {
  cat <<'HELP'
Usage: ./build.sh {linux|windows|macos|all|clean}
  linux    Linux ELF executable (lzss)
  windows  Windows x64 MSVC PE executable (lzss.exe)
  macos    macOS Mach-O executable (lzss-macos)
  all      Build all three targets in parallel
  clean    Remove the three generated executables
Output goes to the implementation directory or LZSS_OUTPUT_DIR.
See README.md for toolchain and SDK setup.
HELP
}

assemble_x86() {
  need nasm 'Install NASM to build the x86-64 assembly implementation.'
  local format=$1 object=$2 source="$asm_dir/lzss-x86_64.asm"
  if [[ $format == win64 ]]; then
    object=$(windows_path "$object")
    source=$(windows_path "$source")
  fi
  nasm -f "$format" -o "$object" "$source"
}

add_asm_source() {
  local arch=$1 format=$2 object="$work/lzss-$2.o"
  case $arch in
    x86_64|amd64)
      assemble_x86 "$format" "$object"
      sources+=("$object") ;;
    aarch64|arm64) sources+=("$asm_dir/lzss-arm64.S") ;;
    *) die "The assembly implementation has no $arch backend." ;;
  esac
}

build_linux() {
  local cc=${CC_LINUX:-${CC:-clang}}
  local sysroot=() sources=("$root/lzss.c") triple
  [[ $host == Linux || -n ${CC_LINUX:-} ]] || die 'Cross-Linux builds require CC_LINUX.'
  need "$cc" 'Install a C23 compiler and Linux development headers.'
  [[ -z ${LINUX_SYSROOT:-} ]] || sysroot=("--sysroot=$LINUX_SYSROOT")
  if [[ $implementation == asm ]]; then
    triple=$("$cc" -dumpmachine)
    add_asm_source "${triple%%-*}" elf64
  fi
  "$cc" "${flags[@]}" ${backend[@]+"${backend[@]}"} ${sysroot[@]+"${sysroot[@]}"} \
    -ffunction-sections -fdata-sections -Wl,--gc-sections "${sources[@]}" -o "$work/lzss"
  publish lzss
}

windows_path() {
  if [[ $host == Windows ]]; then cygpath -m "$1"; else printf '%s\n' "$1"; fi
}

windows_sdk() {
  local cache=${XDG_CACHE_HOME:-$HOME/.cache}
  local shared=${V64TNG_DEPS_CACHE:-$cache/v64tng}/winsdk
  local base=${WINSDK_BASE:-} include lib crt arch candidate sdk
  if [[ -z $base ]]; then
    for base in /opt/winsdk "$shared" "$cache/lzss/winsdk"; do
      [[ ! -d $base/crt/include ]] || break
    done
    if [[ ! -d $base/crt/include ]]; then
      need xwin 'Install xwin or point WINSDK_BASE at an existing xwin SDK.'
      xwin --cache-dir "$cache/lzss/xwin" splat --output "$base"
    fi
  fi
  [[ -d $base ]] || die "Windows SDK directory does not exist at $base."
  base=$(cd "$base" && pwd)
  include=$base/sdk/include
  lib=$base/sdk/lib
  crt=$base/crt
  if [[ ! -d $include/ucrt ]]; then
    # Some SDK installations keep the SDK version between include/lib and um/ucrt.
    for candidate in "$include"/10.*/ucrt; do
      [[ ! -d $candidate ]] || sdk=${candidate%/ucrt}
    done
    include=${sdk:-$include}
  fi
  for candidate in "$lib" "$lib/${include##*/}"; do
    for arch in x86_64 x64; do
      if [[ -f $candidate/um/$arch/kernel32.lib && -f $candidate/ucrt/$arch/libucrt.lib &&
            -f $crt/lib/$arch/libcmt.lib && -d $include/ucrt && -d $include/um ]]; then
        win_headers=("-imsvc$(windows_path "$crt/include")" "-imsvc$(windows_path "$include/ucrt")"
          "-imsvc$(windows_path "$include/um")" "-imsvc$(windows_path "$include/shared")")
        win_libs=("/libpath:$(windows_path "$crt/lib/$arch")"
          "/libpath:$(windows_path "$candidate/um/$arch")" "/libpath:$(windows_path "$candidate/ucrt/$arch")")
        return
      fi
    done
  done
  die "Cannot find x64 Microsoft SDK and CRT files under $base. Set WINSDK_BASE to a complete xwin SDK."
}

build_windows() {
  need clang-cl 'Install LLVM Clang with C23 support.'
  need lld-link 'Install the LLVM linker.'
  local win_headers=() win_libs=() objects=(lzss.obj)
  if [[ $host == Windows ]]; then
    need cygpath 'Run under Git Bash, MSYS2 or Cygwin.'
    export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'
  fi
  if [[ $host != Windows || -n ${WINSDK_BASE:-} ]]; then windows_sdk; fi
  if [[ $implementation == asm ]]; then
    assemble_x86 win64 "$work/lzss-win64.obj"
    objects+=(lzss-win64.obj)
  fi
  (
    cd "$work"
    local target=--target=x86_64-pc-windows-msvc
    clang-cl "$target" /c /TC /clang:-std=c23 /MT /O2 /DNDEBUG /W4 \
      "/DLZSS_CURSOR_BITS=$LZSS_CURSOR_BITS" ${backend[@]+"${backend[@]}"} \
      ${win_headers[@]+"${win_headers[@]}"} \
      -o lzss.obj -- "$(windows_path "$root/lzss.c")"
    clang-cl "$target" -fuse-ld=lld-link "${objects[@]}" -o lzss.exe /link \
      /machine:x64 /subsystem:console /incremental:no /opt:ref /opt:icf /Brepro \
      /defaultlib:libcmt /defaultlib:libvcruntime /defaultlib:libucrt \
      /nodefaultlib:msvcrt.lib /nodefaultlib:ucrt.lib ${win_libs[@]+"${win_libs[@]}"} kernel32.lib
  )
  publish lzss.exe
}

build_macos() {
  local cc=${CC_MACOS:-} sdk=${SDKROOT:-} arch=${MACOS_ARCH:-}
  local min=${MACOSX_DEPLOYMENT_TARGET:-} target=() sources=("$root/lzss.c") triple
  if [[ $host == Darwin ]]; then
    need xcrun 'Install Xcode Command Line Tools with xcode-select --install.'
    cc=${cc:-$(xcrun --find clang)}
    sdk=${sdk:-$(xcrun --sdk macosx --show-sdk-path)}
    [[ -z $arch ]] || target+=(-arch "$arch")
  elif [[ -z $cc ]]; then
    arch=${arch:-x86_64}
    case $arch in
      x86_64) cc=o64-clang ;;
      arm64) cc=oa64-clang ;;
      *) die 'MACOS_ARCH must be x86_64 or arm64.' ;;
    esac
    if ! command -v "$cc" >/dev/null; then
      [[ -n $sdk ]] || die 'Cross-macOS builds require osxcross, CC_MACOS or SDKROOT with Clang and ld64.lld.'
      need ld64.lld 'Install the LLVM Mach-O linker.'
      cc=${CC:-clang}
      target+=("--target=$arch-apple-macos${min:-14.0}" -fuse-ld=lld)
    fi
  fi
  need "$cc" 'Install a macOS Clang toolchain with C23 support.'
  [[ -z $sdk ]] || target+=(-isysroot "$sdk")
  [[ -z $min ]] || target+=("-mmacosx-version-min=$min")
  if [[ $implementation == asm ]]; then
    if [[ -z $arch ]]; then
      triple=$("$cc" -dumpmachine)
      arch=${triple%%-*}
    fi
    add_asm_source "$arch" macho64
  fi
  "$cc" "${flags[@]}" ${backend[@]+"${backend[@]}"} ${target[@]+"${target[@]}"} \
    -Wl,-dead_strip "${sources[@]}" -o "$work/lzss-macos"
  publish lzss-macos
}

publish() {
  mv -f -- "$work/$1" "$out/$1"
  printf 'Built %s\n' "$out/$1"
}

[[ $# -le 1 ]] || die 'Pass one target or use --help.'
case ${1:-help} in
  -h|--help|help) usage; exit ;;
  clean) rm -f -- "$out/lzss" "$out/lzss.exe" "$out/lzss-macos"; exit ;;
  linux|windows|win|macos|mac|all) ;;
  *) usage >&2; exit 1 ;;
esac
case $LZSS_CURSOR_BITS in 32|64) ;; *) die 'LZSS_CURSOR_BITS must be 32 or 64.' ;; esac

host=$(uname -s)
case $host in MINGW*|MSYS*|CYGWIN*) host=Windows ;; esac
mkdir -p -- "$out"
out=$(cd "$out" && pwd)
work=$(mktemp -d "$out/.lzss-build.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
case $1 in
  linux) build_linux ;;
  windows|win) build_windows ;;
  macos|mac) build_macos ;;
  all)
    build_linux & linux_pid=$!
    build_windows <&0 & windows_pid=$!
    build_macos & macos_pid=$!
    status=0
    for pid in "$linux_pid" "$windows_pid" "$macos_pid"; do
      wait "$pid" || status=1
    done
    exit "$status" ;;
esac
