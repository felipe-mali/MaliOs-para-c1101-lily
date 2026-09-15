param([string]$Compiler = "$PSScriptRoot\..\..\.codex_tmp\host-tools\ziglang\zig.exe")
$ErrorActionPreference = 'Stop'
$maliInspectorRoot = (Resolve-Path "$PSScriptRoot\..\..").Path
Push-Location $maliInspectorRoot
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $maliInspectorRoot '.codex_tmp\zig-global'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $maliInspectorRoot '.codex_tmp\zig-local'
    & $Compiler c++ -std=c++17 -O0 -g -Wall -Wextra -I tests/wifi_inspector/stubs tests/wifi_inspector/inspector_test.cpp src/modules/wifi/inspector/device_database.cpp src/modules/wifi/inspector/device_classifier.cpp src/modules/wifi/inspector/oui_database.cpp -o .codex_tmp/inspector_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes locais' }
    & '.\.codex_tmp\inspector_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes locais' }
} finally { Pop-Location }
