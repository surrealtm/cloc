#!/bin/bash
set -eu

# --- Make sure we are in the correct directory
cd "$(dirname "$0")"

# --- Unpack arguments
for arg in "$@"; do declare $arg='1'; done
if [ -v debug ]; then release=0; echo "[Debug Mode]"; fi
if [ -v release ]; then debug=0; echo "[Release Mode]"; fi

# --- Prepare the outp0ut directories
mkdir -p bin

# --- Compile Line Definitions
CLANG_COMMON="../src/cloc.c"
CLANG_DEBUG="clang -O0 -g ${CLANG_COMMON}"
CLANG_RELEASE="clang -O2 ${CLANG_COMMON}"

# --- Build everything
cd bin
if [ -v debug ]; then didbuild=1 && $CLANG_DEBUG; fi
if [ -v release ]; then didbuild=1 && $CLANG_RELEASE; fi
cd ..

   
# --- Warn on No Builds
if [ ! -v didbuild ]
then
    echo "[WARNING] No valid build target specified; specify a target as argument to this script!"
    exit 1
fi

