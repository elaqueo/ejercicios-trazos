# Instalador de Ejercicios y Cartuchera (HU-70): compila Release, junta lo que hace falta
# para correr y genera dist/TrazosSetup-<versión>.exe con Inno Setup.
#   pwsh scripts/package.ps1
# Requiere Inno Setup 6 (winget install JRSoftware.InnoSetup).
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'vsenv.ps1')

# Versión: la del proyecto (CMakeLists.txt raíz).
$version = [regex]::Match((Get-Content (Join-Path $root 'CMakeLists.txt') -Raw), 'project\(\S+ VERSION (\d+\.\d+\.\d+)').Groups[1].Value
if (-not $version) { throw 'No se encontró la versión en CMakeLists.txt' }

& pwsh (Join-Path $PSScriptRoot 'build.ps1') -Preset release
if ($LASTEXITCODE) { throw "El build Release falló ($LASTEXITCODE)" }

# Lo que va al instalador, en build/package: los dos exe con el Qt que despliega
# windeployqt, sin spikes, tests ni restos de builds viejos (la carpeta brushes de libmypaint).
$release = Join-Path $root 'build\release'
$stage = Join-Path $root 'build\package'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory $stage | Out-Null
foreach ($file in 'ejercicios.exe', 'cartuchera.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll',
                  'Qt6Svg.dll', 'dxcompiler.dll', 'dxil.dll') {
    Copy-Item (Join-Path $release $file) $stage
}
foreach ($plugins in 'generic', 'iconengines', 'imageformats', 'networkinformation', 'platforms', 'styles', 'tls') {
    $from = Join-Path $release $plugins
    if (Test-Path $from) { Copy-Item $from (Join-Path $stage $plugins) -Recurse }
}
# Runtime de Visual C++ junto a los exe (despliegue local permitido por Microsoft): así corre
# también donde no está instalado el redistribuible.
$crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
if (-not $crt) { throw "No se encontró el runtime de Visual C++ en $env:VCToolsRedistDir" }
Copy-Item (Join-Path $crt.FullName '*.dll') $stage
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')

$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'No se encontró Inno Setup 6 (winget install JRSoftware.InnoSetup)' }
& $iscc /Q "/DAppVersion=$version" "/DSourceDir=$stage" "/DOutputDir=$(Join-Path $root 'dist')" (Join-Path $root 'installer\trazos.iss')
if ($LASTEXITCODE) { throw "Inno Setup falló ($LASTEXITCODE)" }
Get-Item (Join-Path $root "dist\TrazosSetup-$version.exe") | Select-Object FullName, @{ n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } }
