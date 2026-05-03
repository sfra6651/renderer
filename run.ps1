#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'

Set-Location $PSScriptRoot

$BuildType = 'Debug'
$progArgs = $args
if ($progArgs.Count -gt 0 -and $progArgs[0] -eq '-r') {
    $BuildType = 'Release'
    $progArgs = $progArgs | Select-Object -Skip 1
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error 'cmake not installed'
    exit 1
}

if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Write-Error 'ninja not installed (ships with the Vulkan SDK — check that %VULKAN_SDK%\Bin is on PATH)'
    exit 1
}

if (-not $env:VULKAN_SDK -and -not (Get-Command glslangValidator -ErrorAction SilentlyContinue)) {
    Write-Error 'Vulkan SDK not found. Install from https://vulkan.lunarg.com/'
    exit 1
}

$BuildDir = "build/$BuildType"
cmake -S . -B "$BuildDir" -G Ninja "-DCMAKE_BUILD_TYPE=$BuildType" -Wno-dev
cmake --build "$BuildDir" --parallel

# Keep compile_commands.json at the project root so clangd finds it
$ccj = Join-Path $BuildDir 'compile_commands.json'
if (Test-Path $ccj) {
    Copy-Item -Force $ccj compile_commands.json
}

& .\bin\renderer.exe @progArgs
exit $LASTEXITCODE
