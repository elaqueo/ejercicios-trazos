# Comando único de build (RNF-07): carga MSVC y corre el workflow de CMake.
#   pwsh scripts/build.ps1                  # Debug
#   pwsh scripts/build.ps1 -Preset release
#   pwsh scripts/build.ps1 -Fresh           # reconfigura desde cero
param(
    [ValidateSet('debug', 'release')][string]$Preset = 'debug',
    [switch]$Fresh
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'vsenv.ps1')

Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $workflowArgs = @('--workflow', '--preset', $Preset)
    if ($Fresh) { $workflowArgs += '--fresh' }
    cmake @workflowArgs
    if ($LASTEXITCODE) { throw "cmake --workflow --preset $Preset falló ($LASTEXITCODE)" }
}
finally {
    Pop-Location
}
