#!/bin/bash

# Converts SPIR-V binary to a C++ header with a constexpr  array
# Usage: ./spv_to_header.sh shader.spv [SYMBOL_NAME]

set -e

INPUT="$1"
if [[ ! -f "$INPUT" ]]; then
    echo "File not found: $INPUT"
    exit 1
fi

BASENAME=$(basename "$INPUT")
NAME="${BASENAME%.*}"
SYMBOL="${2:-${NAME^^}_SPV}"  # Default: filename → uppercase + _SPV
OUTPUT="${SYMBOL}.h"

FILE_SIZE=$(stat --format=%s "$INPUT")
WORD_COUNT=$((FILE_SIZE / 4))

echo "Converting $INPUT → $OUTPUT as $SYMBOL ($WORD_COUNT u32)"

{
    echo "#pragma once"
    echo ""
    echo "// Generated from $BASENAME"
    echo "constexpr unsigned int $SYMBOL[$WORD_COUNT] = {"

    # Print 32-bit words in hex
    od -v -An -t x4 "$INPUT" | tr -s ' ' | sed 's/^/    /; s/ /, 0x/g; s/^/0x/' | sed 's/, 0x$//'

    echo "};"
    echo "constexpr unsigned int ${SYMBOL}_SIZE = sizeof($SYMBOL);"
} > "$OUTPUT"

echo "Header created: $OUTPUT"
