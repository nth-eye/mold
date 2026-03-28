#!/bin/bash
# Usage: ./measure.sh <label>
# Rebuilds exec_mold and test_mold with -Os, runs tests, captures detailed binary metrics.
# Works on macOS, Linux, and Windows (MSYS2/MinGW).

LABEL="${1:-unnamed}"
OS="$(uname -s)"

BIN="build-measure/exec_mold"
TEST="build-measure/test_mold"
if [[ "$OS" == MINGW* || "$OS" == MSYS* ]]; then
    BIN="${BIN}.exe"
    TEST="${TEST}.exe"
fi

# Rebuild
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build build-measure --target exec_mold --target test_mold -- -j"$JOBS" > /dev/null 2>&1
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi

# Run tests
$TEST --no-intro --no-version 2>&1 | tail -1

echo "=== $LABEL ==="

# Prefer GNU nm (gnm on macOS via Homebrew binutils) for --radix=d --size-sort
GNM=$(command -v gnm 2>/dev/null || echo "")

echo "--- Parse/Write function sizes ---"
if [[ -n "$GNM" ]]; then
    "$GNM" -C --radix=d --size-sort "$BIN" | grep " [TtWw] " | \
      grep -E 'parse_impl|write_impl|handler_fn|format_handler' | sort -rn
elif [[ "$OS" != Darwin ]]; then
    nm -C --radix=d --size-sort "$BIN" | grep " [TtWw] " | \
      grep -E 'parse_impl|write_impl|handler_fn|format_handler' | sort -rn
else
    # macOS nm: compute sizes from address differences between sorted symbols
    nm -C -n "$BIN" | grep " [TtWw] " | while IFS= read -r line; do
        addr="${line%% *}"; rest="${line#* }"; rest="${rest#* }"; name="$rest"
        printf "%d\t%s\n" "0x$addr" "$name"
    done | awk -F'\t' 'NR>1{printf "%8d %s\n", $1-pa, pn} {pa=$1; pn=$2}' | \
      grep -E 'parse_impl|write_impl|handler_fn|format_handler' | sort -rn
fi

# Section sizes
echo "--- Sections ---"
if [[ "$OS" == Darwin ]]; then
    size -m "$BIN" | grep -E "Section|Segment" | grep -v PAGEZERO | grep -v LINKEDIT
else
    size -A "$BIN" | grep -E '^\.' | sort -k2 -rn
fi

# Symbol counts
TEXT_SYMS=$(nm "$BIN" | grep -c " [TtWw] ")
DATA_SYMS=$(nm "$BIN" | grep -c " [DdSs] ")
echo "--- Symbols ---"
echo "Text symbols: $TEXT_SYMS"
echo "Data symbols: $DATA_SYMS"

echo "--- File size ---"
cp "$BIN" "${BIN}.stripped"
strip "${BIN}.stripped"
if [[ "$OS" == Darwin ]]; then
    echo "Unstripped: $(stat -f '%z' "$BIN") bytes"
    echo "Stripped:   $(stat -f '%z' "${BIN}.stripped") bytes"
else
    echo "Unstripped: $(stat --printf='%s' "$BIN") bytes"
    echo "Stripped:   $(stat --printf='%s' "${BIN}.stripped") bytes"
fi
rm -f "${BIN}.stripped"
echo ""
