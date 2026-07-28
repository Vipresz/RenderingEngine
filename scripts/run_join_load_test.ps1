# Runs the 100-player concurrent join latency engine test and writes a timestamped log.
#
# Usage:
#   .\scripts\run_join_load_test.ps1
#   .\scripts\run_join_load_test.ps1 -Build
#   .\scripts\run_join_load_test.ps1 -Config Debug -MaxP95Ms 3000 -MaxMovementP95Ms 100
#
# Logs: RenderingEngine\scripts\logs\join_load_YYYYMMDD_HHMMSS.log

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string] $Config = "Release",

    [string] $BuildDir = "",

    [switch] $Build,

    [double] $MaxP95Ms = 0,

    [double] $MaxMovementP95Ms = 0
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$EngineRoot = Split-Path -Parent $ScriptDir
$LogsDir = Join-Path $ScriptDir "logs"

function Find-EngineTests {
    param([string] $Root, [string] $Configuration, [string] $PreferredBuildDir)

    $candidates = @()
    if ($PreferredBuildDir) {
        $candidates += Join-Path $Root $PreferredBuildDir
    }
    $candidates += @(
        (Join-Path $Root "build-platform"),
        (Join-Path $Root "build")
    )

    $seen = @{}
    foreach ($dir in $candidates) {
        if (-not $dir -or $seen.ContainsKey($dir)) { continue }
        $seen[$dir] = $true

        $paths = @(
            (Join-Path $dir "bin\$Configuration\EngineTests.exe"),
            (Join-Path $dir "bin\EngineTests.exe"),
            (Join-Path $dir "bin\$Configuration\EngineTests"),
            (Join-Path $dir "bin\EngineTests")
        )
        foreach ($path in $paths) {
            if (Test-Path $path) {
                return (Resolve-Path $path).Path
            }
        }
    }

    return $null
}

function Resolve-BuildDirectory {
    param([string] $Root, [string] $PreferredBuildDir, [string] $EngineTestsPath)

    if ($PreferredBuildDir) {
        return (Join-Path $Root $PreferredBuildDir)
    }

    if ($EngineTestsPath -match "\\build-platform\\") {
        return (Join-Path $Root "build-platform")
    }
    if ($EngineTestsPath -match "\\build\\") {
        return (Join-Path $Root "build")
    }

    if (Test-Path (Join-Path $Root "build-platform")) {
        return (Join-Path $Root "build-platform")
    }
    return (Join-Path $Root "build")
}

if (-not (Test-Path $LogsDir)) {
    New-Item -ItemType Directory -Path $LogsDir | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logPath = Join-Path $LogsDir "join_load_$timestamp.log"
$latestPath = Join-Path $LogsDir "join_load_latest.log"

$engineTests = Find-EngineTests -Root $EngineRoot -Configuration $Config -PreferredBuildDir $BuildDir
if (-not $engineTests) {
    if (-not $Build) {
        throw "EngineTests not found. Build Platform tests first, or re-run with -Build.`n" +
              "  cmake -S . -B build-platform -DBUILD_ENGINE_PLATFORM=ON`n" +
              "  cmake --build build-platform --config $Config --target EngineTests"
    }

    $resolvedBuildDir = if ($BuildDir) { Join-Path $EngineRoot $BuildDir } else { Join-Path $EngineRoot "build-platform" }
    if (-not (Test-Path $resolvedBuildDir)) {
        Write-Host "Configuring $resolvedBuildDir ..."
        & cmake -S $EngineRoot -B $resolvedBuildDir -DBUILD_ENGINE_PLATFORM=ON
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }
    }

    Write-Host "Building EngineTests ($Config) ..."
    & cmake --build $resolvedBuildDir --config $Config --target EngineTests
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

    $engineTests = Find-EngineTests -Root $EngineRoot -Configuration $Config -PreferredBuildDir $BuildDir
    if (-not $engineTests) {
        throw "EngineTests still not found after build."
    }
}

$resolvedBuildDir = Resolve-BuildDirectory -Root $EngineRoot -PreferredBuildDir $BuildDir -EngineTestsPath $engineTests
$filter = "AuthoritativeServerFixture.HundredPlayerConcurrentJoinLatency"

$header = @(
    "AstralWorlds / RenderingEngine - 100-player join load test"
    "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    "Engine root: $EngineRoot"
    "Build dir: $resolvedBuildDir"
    "Config: $Config"
    "Executable: $engineTests"
    "Filter: $filter"
    "JOIN_LOAD_MAX_P95_MS: $(if ($MaxP95Ms -gt 0) { $MaxP95Ms } else { '(not set)' })"
    "JOIN_LOAD_MAX_MOVEMENT_P95_MS: $(if ($MaxMovementP95Ms -gt 0) { $MaxMovementP95Ms } else { '(not set)' })"
    ""
)

$header | Out-File -FilePath $logPath -Encoding utf8
Write-Host "Logging to $logPath"

$previousJoinLoad = $env:ENGINE_RUN_JOIN_LOAD_TEST
$previousMaxP95 = $env:JOIN_LOAD_MAX_P95_MS
$previousMovementP95 = $env:JOIN_LOAD_MAX_MOVEMENT_P95_MS

try {
    $env:ENGINE_RUN_JOIN_LOAD_TEST = "1"
    if ($MaxP95Ms -gt 0) {
        $env:JOIN_LOAD_MAX_P95_MS = [string]$MaxP95Ms
    } else {
        Remove-Item Env:JOIN_LOAD_MAX_P95_MS -ErrorAction SilentlyContinue
    }
    if ($MaxMovementP95Ms -gt 0) {
        $env:JOIN_LOAD_MAX_MOVEMENT_P95_MS = [string]$MaxMovementP95Ms
    } else {
        Remove-Item Env:JOIN_LOAD_MAX_MOVEMENT_P95_MS -ErrorAction SilentlyContinue
    }

    Push-Location (Split-Path -Parent $engineTests)
    try {
        $output = & $engineTests --gtest_filter=$filter 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }

    "" | Out-File -FilePath $logPath -Append -Encoding utf8
    $output | Out-File -FilePath $logPath -Append -Encoding utf8
    "" | Out-File -FilePath $logPath -Append -Encoding utf8
    "Finished: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Out-File -FilePath $logPath -Append -Encoding utf8
    "Exit code: $exitCode" | Out-File -FilePath $logPath -Append -Encoding utf8

    Copy-Item -Path $logPath -Destination $latestPath -Force

    $output | ForEach-Object { Write-Host $_ }

    $joinLine = $output | Where-Object { $_ -match '^\[JoinLoad\]' } | Select-Object -Last 1
    $serverLine = $output | Where-Object { $_ -match '^\[ServerLoad\]' } | Select-Object -Last 1
    if ($joinLine) {
        Write-Host ""
        Write-Host "Join summary: $joinLine"
    }
    if ($serverLine) {
        Write-Host "Server summary: $serverLine"
    }
    $metricsLine = $output | Where-Object { $_ -match '^\[ServerMetrics\]' } | Select-Object -Last 1
    if ($metricsLine) {
        Write-Host "Metrics summary: $metricsLine"
    }

    Write-Host ""
    Write-Host "Log file: $logPath"
    Write-Host "Latest:   $latestPath"

    if ($exitCode -ne 0) {
        exit $exitCode
    }
}
finally {
    if ($null -ne $previousJoinLoad) {
        $env:ENGINE_RUN_JOIN_LOAD_TEST = $previousJoinLoad
    } else {
        Remove-Item Env:ENGINE_RUN_JOIN_LOAD_TEST -ErrorAction SilentlyContinue
    }

    if ($null -ne $previousMaxP95) {
        $env:JOIN_LOAD_MAX_P95_MS = $previousMaxP95
    } else {
        Remove-Item Env:JOIN_LOAD_MAX_P95_MS -ErrorAction SilentlyContinue
    }
    if ($null -ne $previousMovementP95) {
        $env:JOIN_LOAD_MAX_MOVEMENT_P95_MS = $previousMovementP95
    } else {
        Remove-Item Env:JOIN_LOAD_MAX_MOVEMENT_P95_MS -ErrorAction SilentlyContinue
    }
}
