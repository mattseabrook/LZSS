#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
export LZSS_IMPLEMENTATION=asm
export LZSS_OUTPUT_DIR=${LZSS_OUTPUT_DIR:-$root}
exec "$root/../C/build.sh" "$@"
