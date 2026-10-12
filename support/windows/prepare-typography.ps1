# SPDX-License-Identifier: GPL-3.0-or-later
# Prepare verified bundled typography for the Windows PE.
param(
    [string]$OutputDirectory = "build/windows-fonts"
)
$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$out = Join-Path $root $OutputDirectory
$prepare = Join-Path $root "support/tools/prepare_typography.cmake"

& cmake "-DLSM_FONT_OUTPUT_DIR=$out" "-DLSM_FONT_CREATE_ROLE_ALIASES=ON" -P $prepare
if ($LASTEXITCODE -ne 0) {
    throw "Unable to prepare verified Windows typography assets."
}

foreach ($alias in @("ui_regular.ttf", "ui_bold.ttf", "brand_regular.ttf")) {
    if (-not (Test-Path -LiteralPath (Join-Path $out $alias) -PathType Leaf)) {
        throw "Verified typography preparation did not create $alias."
    }
}
