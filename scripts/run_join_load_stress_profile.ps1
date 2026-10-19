# Stress-profile the authoritative join + movement load test at multiple player counts.
#
# Usage:
#   .\scripts\run_join_load_stress_profile.ps1
#   .\scripts\run_join_load_stress_profile.ps1 -Build
#   .\scripts\run_join_load_stress_profile.ps1 -PlayerCounts 64,256,1048
#
# Outputs:
#   scripts\logs\join_load_stress_profile_YYYYMMDD_HHMMSS.json  (full report)
#   scripts\logs\join_load_stress_profile_YYYYMMDD_HHMMSS.txt   (human summary)
#   scripts\logs\join_load_stress_profile_latest.json

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string] $Config = "Release",

    [string] $BuildDir = "",

    [switch] $Build,

    [int[]] $PlayerCounts = @(64, 256, 1048),

    [string] $ScaleConfig = "",

    [switch] $AllowPartialSuccess
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

    $seen = @{ }
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

function ConvertFrom-JoinLoadProfileLine {
    param([string] $Line)

    $prefix = "[JoinLoadProfile] "
    if (-not $Line.StartsWith($prefix)) {
        return $null
    }

    $json = $Line.Substring($prefix.Length).Trim()
    return ($json | ConvertFrom-Json)
}

function Format-LatencySummary {
    param($Latency)

    if ($null -eq $Latency) { return "n/a" }
    return ("count={0} p50={1}ms p95={2}ms p99={3}ms max={4}ms" -f `
        $Latency.count, $Latency.p50_ms, $Latency.p95_ms, $Latency.p99_ms, $Latency.max_ms)
}

if (-not (Test-Path $LogsDir)) {
    New-Item -ItemType Directory -Path $LogsDir | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$jsonPath = Join-Path $LogsDir "join_load_stress_profile_$timestamp.json"
$txtPath = Join-Path $LogsDir "join_load_stress_profile_$timestamp.txt"
$latestJsonPath = Join-Path $LogsDir "join_load_stress_profile_latest.json"
$latestTxtPath = Join-Path $LogsDir "join_load_stress_profile_latest.txt"

$engineTests = Find-EngineTests -Root $EngineRoot -Configuration $Config -PreferredBuildDir $BuildDir
if (-not $engineTests) {
    if (-not $Build) {
        throw "EngineTests not found. Build Platform tests first, or re-run with -Build."
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
$filter = "AuthoritativeServerFixture.ConcurrentJoinLoadStressProfile"

if ($ScaleConfig) {
    $resolvedScaleConfig = $ScaleConfig
} else {
    $resolvedScaleConfig = Join-Path $EngineRoot "config\join_load_stress_scale.json"
}
if (-not (Test-Path $resolvedScaleConfig)) {
    throw "Scale config not found: $resolvedScaleConfig"
}
$resolvedScaleConfig = (Resolve-Path $resolvedScaleConfig).Path

$report = [ordered]@{
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
    engine_root = $EngineRoot
    build_dir = $resolvedBuildDir
    config = $Config
    executable = $engineTests
    scale_config = $resolvedScaleConfig
    player_counts = $PlayerCounts
    runs = @()
}

$summaryLines = @(
    "AstralWorlds / RenderingEngine - join load stress profile"
    "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    "Scale config: $resolvedScaleConfig"
    "Player counts: $($PlayerCounts -join ', ')"
    ""
    ("{0,-8} {1,-8} {2,-8} {3,-12} {4,-12} {5,-10} {6,-8} {7,-8}" -f `
        "Players", "Joined", "MoveOK", "JoinP95", "MoveP95", "TickP95", "Layer", "ShardR")
    ("{0,-8} {1,-8} {2,-8} {3,-12} {4,-12} {5,-10} {6,-8} {7,-8}" -f `
        "--------", "--------", "--------", "------------", "------------", "----------", "--------", "--------")
)

$previousJoinLoad = $env:ENGINE_RUN_JOIN_LOAD_TEST
$previousStressProfile = $env:JOIN_LOAD_STRESS_PROFILE
$previousScaleConfig = $env:JOIN_LOAD_SCALE_CONFIG
$previousPlayerCount = $env:JOIN_LOAD_PLAYER_COUNT
$previousAllowPartial = $env:JOIN_LOAD_ALLOW_PARTIAL_SUCCESS

$overallExitCode = 0

try {
    $env:ENGINE_RUN_JOIN_LOAD_TEST = "1"
    $env:JOIN_LOAD_STRESS_PROFILE = "1"
    $env:JOIN_LOAD_SCALE_CONFIG = $resolvedScaleConfig
    if ($AllowPartialSuccess) {
        $env:JOIN_LOAD_ALLOW_PARTIAL_SUCCESS = "1"
    } else {
        Remove-Item Env:JOIN_LOAD_ALLOW_PARTIAL_SUCCESS -ErrorAction SilentlyContinue
    }

    Push-Location (Split-Path -Parent $engineTests)
    try {
        foreach ($playerCount in $PlayerCounts) {
            Write-Host ""
            Write-Host "=== Stress run: $playerCount players ===" -ForegroundColor Cyan
            $env:JOIN_LOAD_PLAYER_COUNT = [string]$playerCount

            $runStart = Get-Date
            $output = & $engineTests --gtest_filter=$filter 2>&1
            $exitCode = $LASTEXITCODE
            if ($exitCode -ne 0 -and $overallExitCode -eq 0) {
                $overallExitCode = $exitCode
            }

            $profileLine = $output | Where-Object { $_ -match '^\[JoinLoadProfile\]' } | Select-Object -Last 1
            $profile = $null
            if ($profileLine) {
                $profile = ConvertFrom-JoinLoadProfileLine -Line $profileLine
            }

            $runRecord = [ordered]@{
                player_count_target = $playerCount
                exit_code = $exitCode
                duration_ms = [math]::Round(((Get-Date) - $runStart).TotalMilliseconds, 1)
                stdout = @($output)
            }

            if ($profile) {
                $runRecord.profile = $profile
                $summaryLines += ("{0,-8} {1,-8} {2,-8} {3,-12} {4,-12} {5,-10} {6,-8} {7,-8}" -f `
                    $playerCount,
                    $profile.player_count_joined,
                    $profile.player_count_movement_ok,
                    ("{0:N1}" -f $profile.total_join.p95_ms),
                    ("{0:N1}" -f $profile.movement_rtt.p95_ms),
                    ("{0:N1}" -f $profile.server.tick_p95_ms),
                    $profile.server.layering_active,
                    $profile.server.shard_redirect)
            } else {
                $summaryLines += ("{0,-8} {1}" -f $playerCount, "FAILED (no profile line)")
            }

            $report.runs += $runRecord
            $output | ForEach-Object { Write-Host $_ }
        }
    } finally {
        Pop-Location
    }

    $summaryLines += ""
    foreach ($run in $report.runs) {
        if (-not $run.profile) { continue }
        $p = $run.profile
        $summaryLines += "Players $($p.player_count_target): joined $($p.player_count_joined)/$($p.player_count_target), movement ok $($p.player_count_movement_ok)"
        $summaryLines += "  join total   $(Format-LatencySummary $p.total_join)"
        $summaryLines += "  movement rtt $(Format-LatencySummary $p.movement_rtt)"
        $summaryLines += ("  server tick_p95={0}ms density_max={1} layering={2} shard_redirect={3} login_dropped={4} movement_dropped={5}" -f `
            $p.server.tick_p95_ms, $p.server.cell_density_max, $p.server.layering_active, `
            $p.server.shard_redirect, $p.server.login_dropped, $p.server.movement_dropped)
        $summaryLines += ""
    }

    $summaryLines += "Finished: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    $summaryLines += "Overall exit code: $overallExitCode"

    $report | ConvertTo-Json -Depth 12 | Out-File -FilePath $jsonPath -Encoding utf8
    $summaryLines | Out-File -FilePath $txtPath -Encoding utf8

    Copy-Item -Path $jsonPath -Destination $latestJsonPath -Force
    Copy-Item -Path $txtPath -Destination $latestTxtPath -Force

    Write-Host ""
    Write-Host "Stress profile summary" -ForegroundColor Green
    $summaryLines | ForEach-Object { Write-Host $_ }
    Write-Host ""
    Write-Host "JSON report: $jsonPath"
    Write-Host "Text report: $txtPath"
    Write-Host "Latest JSON: $latestJsonPath"

    if ($overallExitCode -ne 0) {
        exit $overallExitCode
    }
}
finally {
    if ($null -ne $previousJoinLoad) {
        $env:ENGINE_RUN_JOIN_LOAD_TEST = $previousJoinLoad
    } else {
        Remove-Item Env:ENGINE_RUN_JOIN_LOAD_TEST -ErrorAction SilentlyContinue
    }
    if ($null -ne $previousStressProfile) {
        $env:JOIN_LOAD_STRESS_PROFILE = $previousStressProfile
    } else {
        Remove-Item Env:JOIN_LOAD_STRESS_PROFILE -ErrorAction SilentlyContinue
    }
    if ($null -ne $previousScaleConfig) {
        $env:JOIN_LOAD_SCALE_CONFIG = $previousScaleConfig
    } else {
        Remove-Item Env:JOIN_LOAD_SCALE_CONFIG -ErrorAction SilentlyContinue
    }
    if ($null -ne $previousPlayerCount) {
        $env:JOIN_LOAD_PLAYER_COUNT = $previousPlayerCount
    } else {
        Remove-Item Env:JOIN_LOAD_PLAYER_COUNT -ErrorAction SilentlyContinue
    }
    if ($null -ne $previousAllowPartial) {
        $env:JOIN_LOAD_ALLOW_PARTIAL_SUCCESS = $previousAllowPartial
    } else {
        Remove-Item Env:JOIN_LOAD_ALLOW_PARTIAL_SUCCESS -ErrorAction SilentlyContinue
    }
}
