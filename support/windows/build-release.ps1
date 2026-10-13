# SPDX-License-Identifier: GPL-3.0-or-later
# Build and qualify the native Windows release artifact.
# Author and maintainer: Shannon Smith

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FailureMessage (exit $LASTEXITCODE)."
    }
}

$candidates = @(
    'C:\msys64\ucrt64\bin\gcc.exe',
    'C:\msys64\mingw64\bin\gcc.exe'
)
$pathGcc = Get-Command gcc.exe -ErrorAction SilentlyContinue
if ($pathGcc) {
    $candidates += $pathGcc.Source
}
$gcc = $candidates |
    Where-Object { $_ -and (Test-Path $_) } |
    Select-Object -First 1
if (-not $gcc) {
    throw 'No native MinGW GCC compiler is available on the Windows runner.'
}

$version = (Get-Content support/VERSION -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Invalid System Monitor version: $version"
}

$repositoryRoot = (Get-Location).Path
$buildDir = Join-Path $repositoryRoot 'build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$versionHeader = Join-Path $buildDir 'windows-version.h'
$fontObject = Join-Path $buildDir 'windows-fonts.o'
$backendProbe = Join-Path $buildDir 'windows-backend-probe.exe'
$releaseExecutable = Join-Path $repositoryRoot "system-monitor-$version-windows.exe"
$startupMarker = Join-Path $repositoryRoot 'windows-startup-smoke.txt'

Set-Content -LiteralPath $versionHeader `
    -Value "#define LSM_VERSION `"$version`"" -Encoding ascii

& (Join-Path $repositoryRoot 'support/windows/prepare-typography.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Windows typography preparation failed.'
}

$windres = Join-Path (Split-Path -Parent $gcc) 'windres.exe'
if (-not (Test-Path $windres)) {
    throw 'No matching MinGW windres is available.'
}
Invoke-Checked -Command $windres -Arguments @(
    '-Isrc',
    'support/windows/system-monitor-fonts.rc',
    '-O', 'coff',
    '-o', $fontObject
) -FailureMessage 'Windows font resource compilation failed'

$windowsSources = Get-Content 'support/source-groups/windows-product.txt' |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -and -not $_.StartsWith('#') } |
    ForEach-Object { "src/$_" }
if (-not $windowsSources) {
    throw 'Windows product source group is empty.'
}

$ninja = Get-Command ninja.exe -ErrorAction SilentlyContinue
if (-not $ninja) {
    throw 'Ninja is required to build the pinned Common library on Windows.'
}
$commonBuild = Join-Path $buildDir 'common-windows'
Invoke-Checked -Command 'cmake' -Arguments @(
    '-S', 'src/infiltratr-common',
    '-B', $commonBuild,
    '-G', 'Ninja',
    "-DCMAKE_MAKE_PROGRAM=$($ninja.Source)",
    "-DCMAKE_C_COMPILER=$gcc",
    '-DCMAKE_BUILD_TYPE=Release',
    '-DINFILTRATR_COMMON_BUILD_TESTS=OFF',
    '-DINFILTRATR_COMMON_BUILD_SHARED=OFF'
) -FailureMessage 'Pinned Common Windows configure failed'
Invoke-Checked -Command 'cmake' -Arguments @(
    '--build', $commonBuild,
    '--target', 'infiltratr-common-portable',
    '--parallel', '2'
) -FailureMessage 'Pinned Common Windows build failed'

$commonLibrary = Join-Path $commonBuild 'libinfiltratr-portable.a'
if (-not (Test-Path $commonLibrary)) {
    throw 'Pinned Common portable library was not produced.'
}

$warningArguments = @(
    '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wshadow', '-Wformat=2',
    '-Wundef', '-Wstrict-prototypes', '-Wmissing-prototypes'
)
$includeArguments = @('-Isrc', '-Isrc/infiltratr-common/include')
$backendLibraries = @(
    '-lpsapi', '-ladvapi32', '-liphlpapi', '-lws2_32', '-lpdh', '-lpowrprof',
    '-ldxgi', '-ldxguid', '-lsetupapi', '-luuid'
)

$guiArguments = @(
    '-std=c17', '-O2', '-D_WIN32_WINNT=0x0601', '-DUNICODE', '-D_UNICODE'
) + $warningArguments + $includeArguments + @(
    '-include', $versionHeader
) + $windowsSources + @(
    $fontObject, $commonLibrary
) + $backendLibraries + @(
    '-lcomctl32', '-lgdi32', '-ldwmapi', '-static-libgcc', '-mwindows',
    '-o', $releaseExecutable
)
Remove-Item -LiteralPath $releaseExecutable -ErrorAction SilentlyContinue
Invoke-Checked -Command $gcc -Arguments $guiArguments `
    -FailureMessage 'Windows GUI compile failed'

$embedded = [Text.Encoding]::ASCII.GetString(
    [IO.File]::ReadAllBytes($releaseExecutable))
if (-not $embedded.Contains($version)) {
    throw "Windows GUI does not contain source version $version."
}

Remove-Item -LiteralPath $startupMarker -ErrorAction SilentlyContinue
$process = $null
$normal = $null
try {
    $process = Start-Process -FilePath $releaseExecutable `
        -WorkingDirectory $repositoryRoot -ArgumentList '--startup-smoke' -PassThru
    if (-not $process.WaitForExit(30000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw 'Windows GUI startup smoke timed out.'
    }
    $status = if (Test-Path -LiteralPath $startupMarker) {
        (Get-Content -LiteralPath $startupMarker -Raw).Trim()
    } else {
        'missing'
    }
    if ($process.ExitCode -ne 0 -or $status -ne 'success') {
        throw "Windows GUI startup failed: exit=$($process.ExitCode), stage=$status"
    }

    $normal = Start-Process -FilePath $releaseExecutable `
        -WorkingDirectory $repositoryRoot -PassThru
    Start-Sleep -Seconds 5
    if ($normal.HasExited) {
        throw "Windows GUI exited during backend startup with code $($normal.ExitCode)."
    }
    Stop-Process -Id $normal.Id -Force
    $normal.WaitForExit()
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    if ($normal -and -not $normal.HasExited) {
        Stop-Process -Id $normal.Id -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $startupMarker -ErrorAction SilentlyContinue
}

$probeArguments = @(
    '-std=c17', '-O2', '-D_WIN32_WINNT=0x0601'
) + $warningArguments + $includeArguments + @(
    'support/tools/windows_backend_probe.c',
    'src/monitor_backend_windows.c',
    'src/process_backend_windows.c',
    'src/windows_process_runtime.c',
    $commonLibrary
) + $backendLibraries + @(
    '-static-libgcc', '-o', $backendProbe
)
Invoke-Checked -Command $gcc -Arguments $probeArguments `
    -FailureMessage 'Windows backend probe compile failed'
Invoke-Checked -Command $backendProbe -Arguments @() `
    -FailureMessage 'Windows backend topology probe failed'
