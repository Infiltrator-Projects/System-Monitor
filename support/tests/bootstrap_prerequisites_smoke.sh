#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Verify the actual bootstrap's missing-tool decisions without package changes.
set -euo pipefail

bootstrap=${1:?bootstrap path required}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

# All commands that the bootstrap needs before its dry-run exit are explicit.
# Deliberately exclude CMake and every test-only compiler/display utility.
ln -s /usr/bin/dirname "$scratch/dirname"
for utility in cc make pkg-config dpkg dpkg-deb; do
    ln -s /usr/bin/true "$scratch/$utility"
done

if full=$(PATH="$scratch" CC=cc /bin/bash "$bootstrap" --dry-run 2>&1); then
    echo 'Bootstrap accepted an incomplete full-verification toolchain.' >&2
    exit 1
fi
for requirement in CMake 'C++ compiler' Clang Doxygen Xvfb xauth; do
    if [[ "$full" != *"$requirement"* ]]; then
        printf 'Bootstrap did not request missing prerequisite: %s\n' "$requirement" >&2
        exit 1
    fi
done
if [[ "$full" != *'Dry run: nothing was installed or changed.'* ]]; then
    echo 'Bootstrap did not preserve dry-run nonmutation.' >&2
    exit 1
fi

if limited=$(PATH="$scratch" CC=cc /bin/bash "$bootstrap" --dry-run --skip-tests 2>&1); then
    echo 'Bootstrap accepted missing CMake under --skip-tests.' >&2
    exit 1
fi
if [[ "$limited" != *CMake* ]]; then
    echo 'Bootstrap did not require CMake for package-only installation.' >&2
    exit 1
fi
for requirement in 'C++ compiler' Clang Doxygen Xvfb xauth; do
    if [[ "$limited" == *"$requirement"* ]]; then
        printf 'Bootstrap requested test-only prerequisite under --skip-tests: %s\n' "$requirement" >&2
        exit 1
    fi
done
if [[ "$limited" != *'Dry run: nothing was installed or changed.'* ]]; then
    echo 'Bootstrap skip-tests path lost dry-run nonmutation.' >&2
    exit 1
fi

echo 'Native bootstrap production/test prerequisite boundaries passed.'
