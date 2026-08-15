#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Minimal pre-compilation bootstrap for the hardware-native installer.
set -Eeuo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)

for argument in "$@"; do
    case "$argument" in
        -h|--help)
            cat <<'USAGE'
Linux System Monitor hardware-native installer
Usage: ./support/installer/bootstrap.sh [options]
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

dry_run=0
for argument in "$@"; do
    [[ "$argument" == --dry-run ]] && dry_run=1
done

find_compiler() {
    local candidate path
    for candidate in "${CC:-}" cc gcc clang; do
        [[ -n "$candidate" ]] || continue
        path=$(command -v -- "$candidate" 2>/dev/null || true)
        if [[ -n "$path" ]]; then
            printf '%s\n' "$path"
            return 0
        fi
    done
    return 1
}

compiler=$(find_compiler || true)
missing=()
packages=()

make_missing=0
command -v make >/dev/null 2>&1 || make_missing=1
if [[ -z "$compiler" ]]; then
    missing+=("C compiler")
fi
if ((make_missing)); then
    missing+=("make")
fi
if [[ -z "$compiler" ]] || ((make_missing)); then
    packages+=(build-essential)
fi

pkg_config=$(command -v pkg-config 2>/dev/null || command -v pkgconf 2>/dev/null || true)
if [[ -z "$pkg_config" ]]; then
    missing+=("pkg-config or pkgconf" "GTK 3.22 development files")
    packages+=(pkg-config libgtk-3-dev)
elif ! "$pkg_config" --exists 'gtk+-3.0 >= 3.22' >/dev/null 2>&1; then
    missing+=("GTK 3.22 development files")
    packages+=(libgtk-3-dev)
fi

dpkg_missing=0
command -v dpkg >/dev/null 2>&1 || dpkg_missing=1
command -v dpkg-deb >/dev/null 2>&1 || dpkg_missing=1
if ((dpkg_missing)); then
    missing+=("Debian packaging tools")
    packages+=(dpkg)
fi

if ((${#missing[@]})); then
    printf 'Missing build requirements:\n' >&2
    printf '  - %s\n' "${missing[@]}" >&2
    if ((dry_run)); then
        printf '\nDry run: nothing was installed or changed.\n' >&2
        exit 1
    fi
    if [[ "${LSM_BOOTSTRAP_PASS:-0}" == 1 ]]; then
        printf '\nRequired build packages are still unavailable after installation.\n' >&2
        exit 1
    fi

    sudo_path=/usr/bin/sudo
    apt_get=/usr/bin/apt-get
    [[ -x "$sudo_path" ]] || sudo_path=
    [[ -x "$apt_get" ]] || apt_get=
    if [[ -z "$sudo_path" || -z "$apt_get" ]]; then
        printf '\nAutomatic prerequisite installation requires sudo and apt-get.\n' >&2
        printf 'Nothing was installed or changed.\n' >&2
        exit 1
    fi

    printf '\nThe installer can install the required packages: %s\n' "${packages[*]}"
    answer=${LSM_AUTO_INSTALL_BUILD_REQUIREMENTS:-}
    if [[ -z "$answer" && -t 0 ]]; then
        read -r -p 'Install the missing build requirements now? [Y/n] ' answer || answer=
    fi
    case "$answer" in
        ""|y|Y|yes|YES|Yes) ;;
        *) printf 'Nothing was installed or changed.\n' >&2; exit 1 ;;
    esac

    printf '\nUpdating package metadata...\n'
    "$sudo_path" -- "$apt_get" update
    printf '\nInstalling missing build requirements...\n'
    "$sudo_path" -- "$apt_get" install -y "${packages[@]}"
    export LSM_BOOTSTRAP_PASS=1
    exec "$ROOT/support/installer/bootstrap.sh" "$@"
fi

compiler=$(find_compiler)
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
