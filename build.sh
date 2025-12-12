#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
# build.sh — LZSS Compression Suite (C23)
# ═══════════════════════════════════════════════════════════════════════════
# Cross-compiles to native Windows EXE (MSVC ABI, no emulation) or Linux ELF
#
# Usage:
#   ./build.sh linux      Native Linux x64 ELF binary
#   ./build.sh windows    Native Windows x64 EXE (no Wine/emulation needed)
#   ./build.sh all        Build both platforms
#   ./build.sh clean      Remove build artifacts
#   ./build.sh            Show help
# ═══════════════════════════════════════════════════════════════════════════
set -Eeuo pipefail
[[ "${LZSS_DEBUG:-0}" == "1" ]] && set -x

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
SRC_FILE="$PROJECT_ROOT/lzss.c"

# ═══════════════════════════════════════════════════════════════════════════
# Color & Emoji Support
# ═══════════════════════════════════════════════════════════════════════════
if [[ -t 1 ]] && command -v tput >/dev/null 2>&1 && [[ $(tput colors 2>/dev/null || echo 0) -ge 8 ]]; then
  COLOR_RESET="\033[0m"
  COLOR_BOLD="\033[1m"
  COLOR_DIM="\033[2m"
  COLOR_RED="\033[31m"
  COLOR_GREEN="\033[32m"
  COLOR_YELLOW="\033[33m"
  COLOR_BLUE="\033[34m"
  COLOR_MAGENTA="\033[35m"
  COLOR_CYAN="\033[36m"
else
  COLOR_RESET="" COLOR_BOLD="" COLOR_DIM="" COLOR_RED="" COLOR_GREEN=""
  COLOR_YELLOW="" COLOR_BLUE="" COLOR_MAGENTA="" COLOR_CYAN=""
fi

if [[ "${LANG:-}" =~ UTF-8 ]] || [[ "${LC_ALL:-}" =~ UTF-8 ]]; then
  EMOJI_SUCCESS="✅" EMOJI_FAILED="❌" EMOJI_WARNING="⚠️"
  EMOJI_ROCKET="🚀" EMOJI_WRENCH="🔧" EMOJI_PACKAGE="📦" EMOJI_FIRE="🔥"
else
  EMOJI_SUCCESS="[OK]" EMOJI_FAILED="[!!]" EMOJI_WARNING="[!]"
  EMOJI_ROCKET=">>>" EMOJI_WRENCH="[*]" EMOJI_PACKAGE="[+]" EMOJI_FIRE="[X]"
fi

banner() {
  local w=60 line
  line="$(printf '%*s' "$w" | tr ' ' '═')"
  printf "\n${COLOR_BOLD}${COLOR_CYAN}╔%s╗\n║ %-*s ║\n╚%s╝${COLOR_RESET}\n" "$line" "$w" "$1" "$line"
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo -e "${COLOR_RED}${EMOJI_FAILED} ERROR: missing '$1'${COLOR_RESET}"
    exit 1
  }
}

on_err() { echo -e "\n${COLOR_RED}${EMOJI_FIRE} Build failed at line $1${COLOR_RESET}"; }
trap 'on_err $LINENO' ERR

# ═══════════════════════════════════════════════════════════════════════════
# Windows SDK Setup (xwin for cross-compilation)
# ═══════════════════════════════════════════════════════════════════════════
WINSDK_BASE="${WINSDK_BASE:-/opt/winsdk}"

setup_winsdk() {
  banner "${EMOJI_PACKAGE} Windows SDK"

  if [[ ! -d "$WINSDK_BASE" ]]; then
    echo -e "${COLOR_YELLOW}${EMOJI_WARNING} Windows SDK not found at $WINSDK_BASE${COLOR_RESET}"
    echo -e "${COLOR_CYAN}Installing via xwin (one-time setup)...${COLOR_RESET}"

    if ! command -v xwin >/dev/null 2>&1; then
      echo -e "${COLOR_CYAN}Installing xwin...${COLOR_RESET}"
      command -v cargo >/dev/null 2>&1 || {
        echo -e "${COLOR_RED}${EMOJI_FAILED} ERROR: cargo not found. Install Rust or xwin manually.${COLOR_RESET}"
        exit 1
      }
      cargo install xwin
    fi

    sudo mkdir -p "$WINSDK_BASE"
    sudo chown "$USER":"$USER" "$WINSDK_BASE"
    xwin --accept-license splat --output "$WINSDK_BASE"
  fi

  # Detect SDK layout (xwin structure)
  SDK_INCLUDE="$WINSDK_BASE/sdk/include"
  SDK_LIB="$WINSDK_BASE/sdk/lib"
  CRT_INCLUDE="$WINSDK_BASE/crt/include"
  CRT_LIB="$WINSDK_BASE/crt/lib"
  LIB_ARCH="x86_64"

  # Find SDK version
  SDK_VERSION=""
  for d in "$SDK_INCLUDE"/*/; do
    [[ -d "$d" ]] && { SDK_VERSION=$(basename "$d"); break; }
  done
  SDK_VERSION="${SDK_VERSION:-10.0.26100}"

  # Validate required paths exist
  local required=(
    "$SDK_INCLUDE/$SDK_VERSION/um"
    "$SDK_INCLUDE/$SDK_VERSION/ucrt"
    "$SDK_INCLUDE/$SDK_VERSION/shared"
    "$SDK_LIB/um/$LIB_ARCH"
    "$SDK_LIB/ucrt/$LIB_ARCH"
    "$CRT_LIB/$LIB_ARCH"
    "$CRT_INCLUDE"
  )
  for p in "${required[@]}"; do
    [[ -d "$p" ]] || {
      echo -e "${COLOR_RED}${EMOJI_FAILED} Missing SDK path: $p${COLOR_RESET}"
      exit 1
    }
  done

  echo -e "${COLOR_GREEN}${EMOJI_SUCCESS} Windows SDK ready (version $SDK_VERSION)${COLOR_RESET}"
}

# ═══════════════════════════════════════════════════════════════════════════
# Build: Linux
# ═══════════════════════════════════════════════════════════════════════════
build_linux() {
  banner "${EMOJI_ROCKET} Build: Linux x64"
  need_cmd clang

  local output="$PROJECT_ROOT/lzss"

  echo -e "${COLOR_DIM}Source:   ${SRC_FILE#$PROJECT_ROOT/}${COLOR_RESET}"
  echo -e "${COLOR_DIM}Target:   Linux x86_64 ELF${COLOR_RESET}"
  echo -e "${COLOR_DIM}Compiler: $(clang --version | head -1)${COLOR_RESET}"
  echo ""

  mkdir -p "$BUILD_DIR"
  local start_time=$(date +%s)

  clang -std=c2x -O3 -DNDEBUG -march=native -fPIC -pipe \
        -Wall -Wextra -Wpedantic -Wno-unused-function -flto \
        -o "$output" "$SRC_FILE"

  local end_time=$(date +%s)
  local duration=$((end_time - start_time))

  [[ -f "$output" ]] || { echo -e "${COLOR_RED}${EMOJI_FAILED} Compilation failed${COLOR_RESET}"; exit 1; }

  local size
  size=$(stat -c%s "$output" 2>/dev/null || stat -f%z "$output")
  local size_fmt
  size_fmt=$(numfmt --to=iec-i --suffix=B <<< "$size" 2>/dev/null || echo "$size bytes")

  echo -e "${COLOR_GREEN}${EMOJI_SUCCESS} Build complete in ${duration}s${COLOR_RESET}"
  echo -e "${COLOR_DIM}   Binary: ${COLOR_BOLD}$output${COLOR_RESET}"
  echo -e "${COLOR_DIM}   Size:   $size_fmt${COLOR_RESET}"
  echo -e "${COLOR_DIM}   Type:   $(file -b "$output" | cut -d, -f1)${COLOR_RESET}"
}

# ═══════════════════════════════════════════════════════════════════════════
# Build: Windows (cross-compile with clang-cl + xwin SDK)
# ═══════════════════════════════════════════════════════════════════════════
build_windows() {
  banner "${EMOJI_ROCKET} Build: Windows x64"
  need_cmd clang-cl
  need_cmd lld-link

  setup_winsdk

  local output="$PROJECT_ROOT/lzss.exe"
  local target="x86_64-pc-windows-msvc"

  echo -e "${COLOR_DIM}Source:   ${SRC_FILE#$PROJECT_ROOT/}${COLOR_RESET}"
  echo -e "${COLOR_DIM}Target:   Windows x86_64 PE/COFF (native MSVC ABI)${COLOR_RESET}"
  echo -e "${COLOR_DIM}Compiler: $(clang-cl --version | head -1)${COLOR_RESET}"
  echo ""

  mkdir -p "$BUILD_DIR"
  local start_time=$(date +%s)

  clang-cl --target="$target" \
    /std:c17 /MT /O2 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /W4 \
    "/imsvc$CRT_INCLUDE" \
    "/imsvc$SDK_INCLUDE/$SDK_VERSION/ucrt" \
    "/imsvc$SDK_INCLUDE/$SDK_VERSION/um" \
    "/imsvc$SDK_INCLUDE/$SDK_VERSION/shared" \
    -o "$output" "$SRC_FILE" \
    /link /subsystem:console /machine:x64 \
    /defaultlib:libcmt /defaultlib:libvcruntime /defaultlib:libucrt \
    /nodefaultlib:msvcrt.lib \
    "/libpath:$SDK_LIB/um/$LIB_ARCH" \
    "/libpath:$SDK_LIB/ucrt/$LIB_ARCH" \
    "/libpath:$CRT_LIB/$LIB_ARCH" \
    kernel32.lib

  local end_time=$(date +%s)
  local duration=$((end_time - start_time))

  # Cleanup .obj artifact
  rm -f "${output%.exe}.obj" 2>/dev/null || true

  [[ -f "$output" ]] || { echo -e "${COLOR_RED}${EMOJI_FAILED} Compilation failed${COLOR_RESET}"; exit 1; }

  local size
  size=$(stat -c%s "$output" 2>/dev/null || stat -f%z "$output")
  local size_fmt
  size_fmt=$(numfmt --to=iec-i --suffix=B <<< "$size" 2>/dev/null || echo "$size bytes")

  echo -e "${COLOR_GREEN}${EMOJI_SUCCESS} Build complete in ${duration}s${COLOR_RESET}"
  echo -e "${COLOR_DIM}   Binary: ${COLOR_BOLD}$output${COLOR_RESET}"
  echo -e "${COLOR_DIM}   Size:   $size_fmt${COLOR_RESET}"
  echo -e "${COLOR_DIM}   Type:   $(file -b "$output" | cut -d, -f1-2)${COLOR_RESET}"
}

# ═══════════════════════════════════════════════════════════════════════════
# Build: All Platforms
# ═══════════════════════════════════════════════════════════════════════════
build_all() {
  build_linux
  echo ""
  build_windows

  banner "${EMOJI_ROCKET} Build Summary"
  echo -e "${COLOR_GREEN}${EMOJI_SUCCESS} Linux:   ./lzss${COLOR_RESET}"
  echo -e "${COLOR_GREEN}${EMOJI_SUCCESS} Windows: ./lzss.exe${COLOR_RESET}"
}

# ═══════════════════════════════════════════════════════════════════════════
# Clean
# ═══════════════════════════════════════════════════════════════════════════
clean() {
  banner "${EMOJI_FIRE} Clean"
  rm -rf "$BUILD_DIR"
  rm -f "$PROJECT_ROOT/lzss" "$PROJECT_ROOT/lzss.exe"
  rm -f "$PROJECT_ROOT"/*.obj "$PROJECT_ROOT"/*.pdb
  echo -e "${COLOR_GREEN}${EMOJI_SUCCESS} Cleaned build artifacts${COLOR_RESET}"
}

# ═══════════════════════════════════════════════════════════════════════════
# Help
# ═══════════════════════════════════════════════════════════════════════════
show_help() {
  cat << 'EOF'
╔════════════════════════════════════════════════════════════════════════════╗
║  LZSS Compression Suite — C23 Cross-Platform Build System                  ║
╚════════════════════════════════════════════════════════════════════════════╝
EOF
  echo -e "${COLOR_CYAN}Usage:${COLOR_RESET}"
  echo "  ./build.sh linux      Build native Linux x64 ELF"
  echo "  ./build.sh windows    Cross-compile native Windows x64 EXE"
  echo "  ./build.sh all        Build both platforms"
  echo "  ./build.sh clean      Remove build artifacts"
  echo ""
  echo -e "${COLOR_CYAN}Examples:${COLOR_RESET}"
  echo "  ./build.sh linux && ./lzss e input.bin output.lzs"
  echo "  ./build.sh windows  # Creates lzss.exe (runs natively on Windows)"
  echo ""
  echo -e "${COLOR_CYAN}Format:${COLOR_RESET}"
  echo "  7th Guest / Retro Game Compatible LZSS"
  echo "  Parameters: N=4096, F=18, THRESHOLD=2"
  echo ""
  echo -e "${COLOR_CYAN}Environment Variables:${COLOR_RESET}"
  echo "  WINSDK_BASE    Windows SDK location (default: /opt/winsdk)"
  echo "  LZSS_DEBUG     Set to 1 for verbose build output"
}

# ═══════════════════════════════════════════════════════════════════════════
# Entry Point
# ═══════════════════════════════════════════════════════════════════════════
cd "$PROJECT_ROOT"

case "${1:-}" in
  linux)          build_linux ;;
  windows|win)    build_windows ;;
  all)            build_all ;;
  clean)          clean ;;
  -h|--help|help) show_help ;;
  *)              show_help ;;
esac
