#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'

Set-Location $PSScriptRoot

$BuildType = 'Debug'
$progArgs = @($args)
if ($progArgs.Count -gt 0 -and $progArgs[0] -eq '-r') {
    $BuildType = 'Release'
    $progArgs = if ($progArgs.Count -gt 1) { $progArgs[1..($progArgs.Count - 1)] } else { @() }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error 'cmake not installed'
}

if (-not $env:VULKAN_SDK -and -not (Get-Command glslangValidator -ErrorAction SilentlyContinue)) {
    Write-Error 'Vulkan SDK not found. Install from https://vulkan.lunarg.com/'
}

$BuildDir = "build/$BuildType"
cmake -S . -B "$BuildDir" "-DCMAKE_BUILD_TYPE=$BuildType" -Wno-dev
cmake --build "$BuildDir" --config $BuildType --parallel

# Keep compile_commands.json at the project root so clangd finds it
$ccj = Join-Path $BuildDir 'compile_commands.json'
if (Test-Path $ccj) {
    Copy-Item -Force $ccj compile_commands.json
}

# Multi-config generators (Visual Studio/MSBuild) put the exe under bin/$BuildType/;
# single-config generators (Ninja, make) put it directly in bin/.
$exe = if (Test-Path "bin/$BuildType/renderer.exe") { "bin/$BuildType/renderer.exe" } else { "bin/renderer.exe" }
& $exe @progArgs
exit $LASTEXITCODE
