#!/usr/bin/env bash
set -euo pipefail

if command -v clang-format >/dev/null 2>&1; then
    FORMATTER="clang-format"
elif command -v xcrun >/dev/null 2>&1 && xcrun --find clang-format >/dev/null 2>&1; then
    FORMATTER="xcrun clang-format"
else
    echo "clang-format not found"
    exit 1
fi

files="$(find src tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) | sort)"

if [ -z "$files" ]; then
    echo "No C++ files found for format check"
    exit 0
fi

status=0
while IFS= read -r file; do
    if ! diff -u "$file" <($FORMATTER "$file") >/dev/null; then
        echo "Formatting mismatch: $file"
        status=1
    fi
done <<EOF
$files
EOF

if [ "$status" -ne 0 ]; then
    echo "Run clang-format on the listed files to fix formatting"
    exit 1
fi

echo "Formatting check passed"
