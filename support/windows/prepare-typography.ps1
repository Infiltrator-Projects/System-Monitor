# SPDX-License-Identifier: GPL-3.0-or-later
# Prepare verified private MB Corpo typography for the native Windows PE.
param(
    [string]$OutputDirectory = "build/windows-fonts"
)
$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$out = Join-Path $root $OutputDirectory
$prepare = Join-Path $root "support/tools/prepare_extended_windows_typography.cmake"

& cmake "-DLSM_FONT_OUTPUT_DIR=$out" -P $prepare
if ($LASTEXITCODE -ne 0) {
    throw "Unable to prepare the six verified Windows typography faces."
}
foreach ($alias in @(
    "ui_regular.ttf", "ui_bold.ttf", "brand_regular.ttf",
    "caption_regular.ttf", "heading_bold.ttf", "metric_regular.ttf"
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $out $alias) -PathType Leaf)) {
        throw "Verified Windows typography is missing $alias."
    }
}
