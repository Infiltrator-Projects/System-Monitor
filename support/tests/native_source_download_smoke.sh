#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Network-free test of the actual generated native installer.
set -Eeuo pipefail
installer="$1"
umask 077
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/project/System-Monitor-fixture/support/installer" \
         "$work/common/Libraries-fixture" "$work/bin"
cp support/VERSION "$work/project/System-Monitor-fixture/support/VERSION"
printf '1.19-fixture\n' > "$work/common/Libraries-fixture/VERSION"
cat > "$work/project/System-Monitor-fixture/support/installer/bootstrap.sh" <<'BOOTSTRAP'
#!/usr/bin/env bash
set -Eeuo pipefail
test -s "$LSM_SOURCE_ROOT/src/infiltratr-common/VERSION"
printf 'bootstrapped\n' >> "$SMOKE_BOOTSTRAP_MARKER"
BOOTSTRAP
chmod 0755 "$work/project/System-Monitor-fixture/support/installer/bootstrap.sh"
tar -czf "$work/source.tar.gz" -C "$work/project" System-Monitor-fixture
tar -czf "$work/common.tar.gz" -C "$work/common" Libraries-fixture
cat > "$work/bin/curl" <<'MOCK'
#!/usr/bin/env bash
set -Eeuo pipefail
destination=
url=
while (( $# > 0 )); do
    case "$1" in
        --output) destination="$2"; shift 2 ;;
        *) url="$1"; shift ;;
    esac
done
test -n "$destination" && test -n "$url"
printf '%s\n' "$url" >> "$SMOKE_URL_LOG"
case "$url" in
    https://github.com/Infiltrator-Projects/System-Monitor/archive/*)
        if [[ "$SMOKE_SCENARIO" == source-fallback ]]; then exit 22; fi
        cp -- "$SMOKE_SOURCE_TAR" "$destination" ;;
    https://codeload.github.com/Infiltrator-Projects/System-Monitor/tar.gz/*)
        cp -- "$SMOKE_SOURCE_TAR" "$destination" ;;
    https://github.com/Infiltrator-Projects/Libraries/archive/*)
        if [[ "$SMOKE_SCENARIO" == corrupt ]]; then
            printf 'not a tarball\n' > "$destination"
            exit 0
        fi
        exit 22 ;;
    https://codeload.github.com/Infiltrator-Projects/Libraries/tar.gz/*)
        if [[ "$SMOKE_SCENARIO" == exhausted ]]; then exit 22; fi
        cp -- "$SMOKE_COMMON_TAR" "$destination" ;;
    *) printf 'Unexpected network target: %s\n' "$url" >&2; exit 90 ;;
esac
MOCK
chmod 0755 "$work/bin/curl"
set --
if (( EUID == 0 )); then set -- --system-package-mode; fi
for scenario in gateway corrupt source-fallback exhausted; do
    : > "$work/urls"
    : > "$work/bootstrap-marker"
    if env PATH="$work/bin:$PATH" SMOKE_SCENARIO="$scenario" \
        SMOKE_SOURCE_TAR="$work/source.tar.gz" \
        SMOKE_COMMON_TAR="$work/common.tar.gz" \
        SMOKE_URL_LOG="$work/urls" \
        SMOKE_BOOTSTRAP_MARKER="$work/bootstrap-marker" \
        /bin/bash "$installer" "$@" > "$work/output" 2>&1; then
        result=0
    else
        result=$?
    fi
    if [[ "$scenario" == exhausted ]]; then
        test "$result" -ne 0 || { echo "Missing Common source was accepted" >&2; exit 1; }
        test ! -s "$work/bootstrap-marker" || { echo "Failed acquisition ran bootstrap" >&2; exit 1; }
        grep -Fq 'Unable to download the shared-library source' "$work/output"
    else
        test "$result" -eq 0 || { cat "$work/output" >&2; exit 1; }
        grep -Fxq 'bootstrapped' "$work/bootstrap-marker" || { cat "$work/output" >&2; exit 1; }
    fi
    grep -Fq 'https://codeload.github.com/Infiltrator-Projects/Libraries/tar.gz/' "$work/urls"
    if [[ "$scenario" == source-fallback ]]; then
        grep -Fq 'https://codeload.github.com/Infiltrator-Projects/System-Monitor/tar.gz/' "$work/urls"
    fi
done
printf 'Native archive gateway, invalid payload, fallback and exhaustion tests passed.\n'
