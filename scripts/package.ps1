param(
    [string]$BuildDir = "cmake-build-msvc-release",
    [string]$OutputDir = "dist\TrackCamHub",
    [string]$Configuration = "Release",
    [switch]$SkipVCRuntime
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildPath = Join-Path $RepoRoot $BuildDir
$OutputPath = Join-Path $RepoRoot $OutputDir

function Copy-DllDirectory {
    param(
        [string]$SourceDir,
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $SourceDir)) {
        throw "Cannot find $Description directory: $SourceDir"
    }

    Copy-Item -Path (Join-Path $SourceDir "*.dll") -Destination $OutputPath -Force
    Write-Host "Copied $Description from: $SourceDir"
}

cmake -S $RepoRoot -B $BuildPath -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration"
cmake --build $BuildPath --config $Configuration

if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $OutputPath "config") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $OutputPath "docs") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $OutputPath "logs") | Out-Null

Copy-Item -LiteralPath (Join-Path $BuildPath "TrackCamHub.exe") -Destination $OutputPath
Copy-Item -Path (Join-Path $BuildPath "*.dll") -Destination $OutputPath -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $RepoRoot "config\trackcamhub.ini") -Destination (Join-Path $OutputPath "config")
Copy-Item -LiteralPath (Join-Path $RepoRoot "README.md") -Destination $OutputPath
Copy-Item -Path (Join-Path $RepoRoot "docs\*") -Destination (Join-Path $OutputPath "docs") -Recurse
Copy-Item -Path (Join-Path $PSScriptRoot "runtime\*") -Destination $OutputPath -Recurse

if (-not $SkipVCRuntime) {
    if (-not $env:VCToolsRedistDir) {
        throw "VCToolsRedistDir is not set. Run this script from a Visual Studio Developer PowerShell/Command Prompt, or pass -SkipVCRuntime if you intentionally do not want local VC runtime DLLs."
    }

    $VcCrtDir = Join-Path $env:VCToolsRedistDir "x64\Microsoft.VC143.CRT"
    Copy-DllDirectory -SourceDir $VcCrtDir -Description "MSVC runtime"

    if ($env:UniversalCRTSdkDir) {
        $UcrtDir = Join-Path $env:UniversalCRTSdkDir "Redist\ucrt\DLLs\x64"
        if (Test-Path -LiteralPath $UcrtDir) {
            Copy-DllDirectory -SourceDir $UcrtDir -Description "Universal CRT"
        } else {
            $RedistRoot = Join-Path $env:UniversalCRTSdkDir "Redist"
            $VersionedUcrtDir = Get-ChildItem -LiteralPath $RedistRoot -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { Join-Path $_.FullName "ucrt\DLLs\x64" } |
                Where-Object { Test-Path -LiteralPath $_ } |
                Sort-Object -Descending |
                Select-Object -First 1

            if ($VersionedUcrtDir) {
                Copy-DllDirectory -SourceDir $VersionedUcrtDir -Description "Universal CRT"
            } else {
                Write-Warning "Universal CRT redist directory was not found under: $RedistRoot"
            }
        }
    } else {
        Write-Warning "UniversalCRTSdkDir is not set; Universal CRT DLLs were not copied."
    }
}

Write-Host "Packaged TrackCamHub to: $OutputPath"
Write-Host "Console:"
Write-Host "  $OutputPath\run_console.bat"
Write-Host "Install service as Administrator:"
Write-Host "  $OutputPath\install_service.bat"
