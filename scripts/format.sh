#!/usr/bin/env bash
# Formats the project's C++ sources with clang-format.
#
#   scripts/format.sh            rewrite files in place
#   scripts/format.sh --check    report unformatted files, change nothing (exit 1 if any)
#
# third_party/ is excluded -- vendored code keeps upstream's style.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

CHECK=0
[[ "${1:-}" == "--check" ]] && CHECK=1

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
if ! command -v "${CLANG_FORMAT}" >/dev/null 2>&1; then
  echo "error: ${CLANG_FORMAT} not found." >&2
  echo "  Debian/Ubuntu: sudo apt install clang-format" >&2
  echo "  Arch:          sudo pacman -S clang" >&2
  exit 127
fi

mapfile -t FILES < <(find src tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.in' \) | sort)

if [[ "${#FILES[@]}" -eq 0 ]]; then
  echo "No C++ sources found."
  exit 0
fi

if [[ "${CHECK}" -eq 1 ]]; then
  unformatted=()
  for f in "${FILES[@]}"; do
    if ! "${CLANG_FORMAT}" --style=file "${f}" | diff -q - "${f}" >/dev/null 2>&1; then
      unformatted+=("${f}")
    fi
  done

  if [[ "${#unformatted[@]}" -gt 0 ]]; then
    echo "Not formatted:"
    printf '  %s\n' "${unformatted[@]}"
    echo
    echo "Run: scripts/format.sh"
    exit 1
  fi
  echo "All ${#FILES[@]} files are formatted (${CLANG_FORMAT##*/} $("${CLANG_FORMAT}" --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1))."
  exit 0
fi

"${CLANG_FORMAT}" --style=file -i "${FILES[@]}"
echo "Formatted ${#FILES[@]} files."
