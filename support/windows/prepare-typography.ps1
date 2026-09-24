# SPDX-License-Identifier: GPL-3.0-or-later
# Prepare Common's canonical verified typography assets for the Windows PE.
param(
    [string]$OutputDirectory = "build/windows-fonts"
)
$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$meta = Join-Path $root "src/infiltratr-common/cmake/InfiltratrTypographyAssets.cmake"
$out = Join-Path $root $OutputDirectory
New-Item -ItemType Directory -Force -Path $out | Out-Null

$lines = Get-Content -LiteralPath $meta
function Get-CMakeValue([string]$Key) {
    for ($i = 0; $i -lt $lines.Count - 1; $i++) {
        if ($lines[$i] -match ("^set\(" + [regex]::Escape($Key) + "$")) {
            return ($lines[$i + 1] -replace '^[\s"]+|["\)\s]+$', '')
        }
    }
    throw "Missing Common typography metadata: $Key"
}

$sourceCommit = Get-CMakeValue "INFILTRATR_MB_CORPO_SOURCE_COMMIT"
$urlTemplate = Get-CMakeValue "INFILTRATR_MB_CORPO_ARCHIVE_URL"
$archiveSha = (Get-CMakeValue "INFILTRATR_MB_CORPO_ARCHIVE_SHA256").ToLowerInvariant()
$uiRegularFile = Get-CMakeValue "INFILTRATR_MB_CORPO_UI_REGULAR_FILE"
$uiRegularSha = (Get-CMakeValue "INFILTRATR_MB_CORPO_UI_REGULAR_SHA256").ToLowerInvariant()
$uiBoldFile = Get-CMakeValue "INFILTRATR_MB_CORPO_UI_BOLD_FILE"
$uiBoldSha = (Get-CMakeValue "INFILTRATR_MB_CORPO_UI_BOLD_SHA256").ToLowerInvariant()
$brandFile = Get-CMakeValue "INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE"
$brandSha = (Get-CMakeValue "INFILTRATR_MB_CORPO_BRAND_REGULAR_SHA256").ToLowerInvariant()

$url = $urlTemplate.Replace('${INFILTRATR_MB_CORPO_SOURCE_COMMIT}', $sourceCommit)
$archive = Join-Path $out "mb-corpo-fonts.tar.xz"
Invoke-WebRequest -Uri $url -OutFile $archive
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant() -ne $archiveSha) {
    throw "Canonical MB Corpo archive hash mismatch."
}
tar -xf $archive -C $out
if ($LASTEXITCODE -ne 0) { throw "Unable to extract canonical MB Corpo archive." }

function Copy-VerifiedFont([string]$Filename, [string]$Expected, [string]$Target) {
    $source = Get-ChildItem -LiteralPath $out -Recurse -File |
        Where-Object { $_.Name -eq $Filename } |
        Select-Object -First 1
    if (-not $source) { throw "Font $Filename was not present in the verified archive." }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $source.FullName).Hash.ToLowerInvariant() -ne $Expected) {
        throw "Font hash mismatch for $Filename."
    }
    Copy-Item -LiteralPath $source.FullName -Destination (Join-Path $out $Target) -Force
}

Copy-VerifiedFont $uiRegularFile $uiRegularSha "ui_regular.ttf"
Copy-VerifiedFont $uiBoldFile $uiBoldSha "ui_bold.ttf"
Copy-VerifiedFont $brandFile $brandSha "brand_regular.ttf"
