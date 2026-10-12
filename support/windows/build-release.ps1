# SPDX-License-Identifier: GPL-3.0-or-later
# Build and qualify the native Windows release artifact.
# Author and maintainer: Shannon Smith

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][object[]]$Arguments,
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

New-Item -ItemType Directory -Force -Path build | Out-Null
$versionHeader = Join-Path $PWD 'windows-version.h'
Set-Content -LiteralPath $versionHeader `
    -Value "#define LSM_VERSION `"$version`"" -Encoding ascii

& (Join-Path $PWD 'support/windows/prepare-typography.ps1')
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
    '-o', 'build/windows-fonts.o'
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
$commonBuild = Join-Path $PWD 'build/common-windows'
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
    'build/windows-fonts.o', $commonLibrary
) + $backendLibraries + @(
    '-lcomctl32', '-lgdi32', '-ldwmapi', '-static-libgcc', '-mwindows',
    '-o', 'system-monitor-windows.exe'
)
Invoke-Checked -Command $gcc -Arguments $guiArguments `
    -FailureMessage 'Windows GUI compile failed'

$embedded = [Text.Encoding]::ASCII.GetString(
    [IO.File]::ReadAllBytes('system-monitor-windows.exe'))
if (-not $embedded.Contains($version)) {
    throw "Windows GUI does not contain source version $version."
}

Remove-Item windows-startup-smoke.txt -ErrorAction SilentlyContinue
$process = Start-Process -FilePath (Join-Path $PWD 'system-monitor-windows.exe') `
    -ArgumentList '--startup-smoke' -PassThru
if (-not $process.WaitForExit(30000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw 'Windows GUI startup smoke timed out.'
}
$status = if (Test-Path windows-startup-smoke.txt) {
    (Get-Content windows-startup-smoke.txt -Raw).Trim()
} else {
    'missing'
}
if ($process.ExitCode -ne 0 -or $status -ne 'success') {
    throw "Windows GUI startup failed: exit=$($process.ExitCode), stage=$status"
}

$normal = Start-Process -FilePath (Join-Path $PWD 'system-monitor-windows.exe') `
    -PassThru
Start-Sleep -Seconds 5
if ($normal.HasExited) {
    throw "Windows GUI exited during backend startup with code $($normal.ExitCode)."
}
Stop-Process -Id $normal.Id -Force
$normal.WaitForExit()

$probeArguments = @(
    '-std=c17', '-O2', '-D_WIN32_WINNT=0x0601'
) + $warningArguments + $includeArguments + @(
    'support/tools/windows_backend_probe.c',
    'src/monitor_backend_windows.c',
    'src/process_backend_windows.c',
    $commonLibrary
) + $backendLibraries + @(
    '-static-libgcc', '-o', 'windows-backend-probe.exe'
)
Invoke-Checked -Command $gcc -Arguments $probeArguments `
    -FailureMessage 'Windows backend probe compile failed'
Invoke-Checked -Command (Join-Path $PWD 'windows-backend-probe.exe') `
    -Arguments @() -FailureMessage 'Windows backend topology probe failed'

Move-Item 'system-monitor-windows.exe' "system-monitor-$version-windows.exe"
