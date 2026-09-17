#!/usr/bin/env bash
# Points git at the version-controlled hooks in .githooks/, so the whole repo
# shares one set rather than each clone configuring its own.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
git -C "${ROOT}" config core.hooksPath .githooks

echo "Hooks installed: core.hooksPath -> .githooks"
echo
echo "Active hooks:"
for h in "${ROOT}"/.githooks/*; do
  [[ -f "$h" ]] && printf '  %s%s\n' "$(basename "$h")" \
    "$([[ -x "$h" ]] || echo '   (NOT EXECUTABLE -- run chmod +x)')"
done
echo
echo "Disable with: git config --unset core.hooksPath"
