param(
    [string]$BuildDir = "build-cov",
    [string]$Config = "Debug",
    [double]$MinCoverage = 0.60,
    [string]$OpenCppCoverage = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$engineRoot = $repoRoot

if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $buildRoot = $BuildDir
} else {
    $buildRoot = Join-Path $engineRoot $BuildDir
}

$testExe = Join-Path $buildRoot "bin\$Config\EngineTests.exe"
if (-not (Test-Path $testExe)) {
    $found = Get-ChildItem -Path $buildRoot -Recurse -Filter "EngineTests.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($found) {
        $testExe = $found.FullName
    }
}
$coverageXml = Join-Path $buildRoot "coverage.xml"

if (-not (Test-Path $testExe)) {
    Write-Error "EngineTests not found at $testExe. Build with: cmake -B $buildRoot -DCMAKE_BUILD_TYPE=$Config -DBUILD_ENGINE_EXAMPLES=OFF -DBUILD_ENGINE_TESTS=ON"
}

if (-not (Test-Path $OpenCppCoverage)) {
    Write-Error "OpenCppCoverage not found at '$OpenCppCoverage'. Install from https://github.com/OpenCppCoverage/OpenCppCoverage/releases"
}

$sourceRoot = $engineRoot
$exclude = @(
    "$sourceRoot\third_party",
    "$sourceRoot\tests",
    $buildRoot
)

$occArgs = @(
    "--sources", $sourceRoot
)
foreach ($path in $exclude) {
    $occArgs += "--excluded_sources"
    $occArgs += $path
}
$occArgs += @(
    "--export_type", "cobertura:$coverageXml",
    "--", $testExe
)

Write-Host "Running OpenCppCoverage on $testExe ..."
& $OpenCppCoverage @occArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "OpenCppCoverage failed with exit code $LASTEXITCODE (see EngineTests output above for failing cases)"
}

[xml]$report = Get-Content $coverageXml
$lineRate = [double]$report.coverage.'line-rate'
$linesValid = [int]$report.coverage.'lines-valid'
$linesCovered = [int]$report.coverage.'lines-covered'
$pct = [math]::Round($lineRate * 100, 2)

Write-Host "Line coverage: $linesCovered / $linesValid ($pct%)"
Write-Host "Minimum required: $([math]::Round($MinCoverage * 100, 2))%"

if ($lineRate -lt $MinCoverage) {
    Write-Error "Coverage $pct% is below minimum $([math]::Round($MinCoverage * 100, 2))%"
}

Write-Host "Coverage gate passed."
