#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Pre-compilation bootstrap for the hardware-native system installer.
#
# No project executable exists when this file starts. It finds one C compiler,
# compiles the audited C17 native installer into a private temporary
# directory, and immediately hands all remaining work to that C program.
set -Eeuo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

for argument in "$@"; do
    case "$argument" in
        -h|--help)
            cat <<'USAGE'
Linux System Monitor hardware-native installer

Usage: ./install.sh [options]

Options:
  --profile native|aggressive|portable
  --compiler PATH
  --jobs NUMBER
  --skip-tests
  --no-strip
  --dry-run
  -h, --help
USAGE
            exit 0
            ;;
    esac
done

if ((EUID == 0)); then
    printf 'Error: do not run this builder with sudo or as root\n' >&2
    exit 1
fi

compiler=${CC:-}
if [[ -n "$compiler" ]]; then
    compiler=$(command -v -- "$compiler" 2>/dev/null || true)
else
    for candidate in cc gcc clang; do
        compiler=$(command -v -- "$candidate" 2>/dev/null || true)
        [[ -n "$compiler" ]] && break
    done
fi
if [[ -z "$compiler" ]]; then
    printf 'Missing build requirements:\n  - C compiler\n\nNothing was installed or changed.\n' >&2
    exit 1
fi

temporary=$(mktemp -d "${TMPDIR:-/tmp}/lsm-bootstrap-XXXXXX")
trap 'rm -rf -- "$temporary"' EXIT
builder="$temporary/native-installer"

"$compiler" -std=c17 -O2 \
    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 -Wundef \
    -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
    -Wswitch-enum -Wnull-dereference \
    "$ROOT/support/tools/native_installer.c" -o "$builder"

export LSM_SOURCE_ROOT="$ROOT"
export LSM_BOOTSTRAP_CC="$compiler"
export LSM_BOOTSTRAP_BINARY="$builder"
trap - EXIT
exec "$builder" "$@"
