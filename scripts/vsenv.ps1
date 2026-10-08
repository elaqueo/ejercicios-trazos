# Carga en la sesión actual el entorno de MSVC x64 de VS 2022 (cl, link, Windows SDK
# y el cmake/ninja que trae Build Tools). Uso:  . .\scripts\vsenv.ps1
# No modifica nada del sistema: solo variables de entorno de esta sesión.

if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64') { return }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "No se encontró vswhere.exe: ¿están instaladas las VS 2022 Build Tools?" }

$vsPath = & $vswhere -latest -products * -version '[17.0,18.0)' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "No se encontró VS 2022 / Build Tools con MSVC x64 (v143)." }

& (Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
