#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Prepare Common's canonical verified typography assets for the Windows PE.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
meta="$root/src/infiltratr-common/cmake/InfiltratrTypographyAssets.cmake"
out="${1:-$root/build/windows-fonts}"
mkdir -p "$out"
rm -f "$out"/*.ttf "$out"/mb-corpo-fonts.tar.xz

cmake_value() {
    local key="$1"
    awk -v key="$key" '
        $0 ~ "set\\(" key {
            if (getline line) {
                gsub(/^[[:space:]]*"/, "", line)
                gsub(/"[[:space:]]*\).*/, "", line)
                print line
                exit
            }
        }
    ' "$meta"
}

source_commit="$(cmake_value INFILTRATR_MB_CORPO_SOURCE_COMMIT)"
url_template="$(cmake_value INFILTRATR_MB_CORPO_ARCHIVE_URL)"
archive_sha="$(cmake_value INFILTRATR_MB_CORPO_ARCHIVE_SHA256)"
ui_regular_file="$(cmake_value INFILTRATR_MB_CORPO_UI_REGULAR_FILE)"
ui_regular_sha="$(cmake_value INFILTRATR_MB_CORPO_UI_REGULAR_SHA256)"
ui_bold_file="$(cmake_value INFILTRATR_MB_CORPO_UI_BOLD_FILE)"
ui_bold_sha="$(cmake_value INFILTRATR_MB_CORPO_UI_BOLD_SHA256)"
brand_file="$(cmake_value INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE)"
brand_sha="$(cmake_value INFILTRATR_MB_CORPO_BRAND_REGULAR_SHA256)"

for value in "$source_commit" "$url_template" "$archive_sha"              "$ui_regular_file" "$ui_regular_sha"              "$ui_bold_file" "$ui_bold_sha"              "$brand_file" "$brand_sha"; do
    test -n "$value"
done

archive_url="${url_template//\${INFILTRATR_MB_CORPO_SOURCE_COMMIT}/$source_commit}"
archive="$out/mb-corpo-fonts.tar.xz"
curl -fsSL --retry 5 "$archive_url" -o "$archive"
printf '%s  %s\n' "$archive_sha" "$archive" | sha256sum -c -
tar -xJf "$archive" -C "$out"

copy_verified() {
    local filename="$1"
    local expected="$2"
    local target="$3"
    local source
    source="$(find "$out" -type f -name "$filename" -print -quit)"
    test -n "$source"
    printf '%s  %s\n' "$expected" "$source" | sha256sum -c -
    install -m 0644 "$source" "$out/$target"
}

copy_verified "$ui_regular_file" "$ui_regular_sha" ui_regular.ttf
copy_verified "$ui_bold_file" "$ui_bold_sha" ui_bold.ttf
copy_verified "$brand_file" "$brand_sha" brand_regular.ttf
