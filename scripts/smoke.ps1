# Prueba de la app real (HU-71): abre Ejercicios (Release), la maneja con teclado como lo haría
# el usuario y verifica en su log lo que pasó. Encuentra lo que los tests de lógica no ven
# (teclas, foco, menús: el cursor del menú que daba la vuelta, HU-20).
#   pwsh scripts/smoke.ps1             # usa build/release/ejercicios.exe (compilarlo antes)
# Toma la pantalla unos 20 s: no tocar el teclado mientras corre. Deja config.json como estaba.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'build\release\ejercicios.exe'
if (-not (Test-Path $exe)) { throw "Falta ${exe}: compilar con pwsh scripts/build.ps1 -Preset release" }
if (Get-Process ejercicios -ErrorAction SilentlyContinue) { throw 'Ejercicios está abierto: cerrarlo antes de la prueba' }

$data = Join-Path $env:LOCALAPPDATA 'trazos'
$config = Join-Path $data 'config.json'
$log = Join-Path $data 'ejercicios.log'
$backup = if (Test-Path $config) { [IO.File]::ReadAllBytes($config) } else { $null }

# Estado conocido: la recta, sin modo mixto.
$state = if ($backup) { Get-Content $config -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
if (-not $state.ContainsKey('ejercicios')) { $state['ejercicios'] = @{} }
$state['ejercicios']['exercise'] = 'recta'
$state['ejercicios']['mixed'] = $false
New-Item -ItemType Directory -Force $data | Out-Null
$state | ConvertTo-Json -Depth 32 | Set-Content $config -Encoding utf8

$failures = [System.Collections.Generic.List[string]]::new()
$app = $null
try {
    $app = Start-Process $exe -PassThru
    Start-Sleep 4
    $ws = New-Object -ComObject WScript.Shell
    $null = $ws.AppActivate($app.Id)
    Start-Sleep 1
    function Send([string]$keys, [int]$times = 1, [int]$pauseMs = 250) {
        for ($i = 0; $i -lt $times; ++$i) { $ws.SendKeys($keys); Start-Sleep -Milliseconds $pauseMs }
    }
    # Líneas del log desde la última revisada.
    $seen = 0
    function Take() {
        Start-Sleep -Milliseconds 400
        $lines = @(Get-Content $log -Encoding utf8 | Where-Object { $_ -match 'Menú:|Ejercicio:|Repetir:|Guías:|Panel:' })
        $new = $lines | Select-Object -Skip $script:seen
        $script:seen = $lines.Count
        return @($new)
    }
    function Check([string]$name, [bool]$ok, $detail) {
        if ($ok) { Write-Host "  ok   $name" } else { Write-Host "  FALLA $name · $detail"; $failures.Add($name) }
    }
    $null = Take

    Send '{RIGHT}' 2
    $l = Take
    Check '→ pasa al ejercicio siguiente' (($l | Where-Object { $_ -match 'Ejercicio: "recta"' }).Count -eq 2) ($l -join ' | ')

    Send 'r'
    $l = Take
    Check 'R repite el ejercicio' ($l -match 'Repetir: "recta"').Count ($l -join ' | ')

    Send 'g' 2
    $l = Take
    Check 'G oculta y vuelve a mostrar las guías' (($l -join ' ') -match 'ocultas.*visibles') ($l -join ' | ')

    # Menú: subir de más se frena en el modo mixto (antes daba la vuelta).
    Send '{F4}' 1 600
    Send '{UP}' 10 120
    Send '{ENTER}' 1 600
    $l = Take
    Check 'F4 + ↑ de más + Enter elige el modo mixto' ($l -match 'Menú: "mixto"').Count ($l -join ' | ')

    Send '{RIGHT}' 8 350
    $l = @(Take | Where-Object { $_ -match 'Ejercicio:' })
    $ids = @($l | ForEach-Object { if ($_ -match 'Ejercicio: "([^"]+)"') { $Matches[1] } })
    $allMixed = ($l | Where-Object { $_ -notmatch 'modo mixto' }).Count -eq 0
    $threeInARow = $false
    for ($i = 2; $i -lt $ids.Count; ++$i) { if ($ids[$i] -eq $ids[$i - 1] -and $ids[$i] -eq $ids[$i - 2]) { $threeInARow = $true } }
    Check 'en modo mixto → sortea, sin tres seguidos iguales' ($ids.Count -eq 8 -and $allMixed -and -not $threeInARow) ($ids -join ',')
    Check 'en modo mixto salen ejercicios distintos' (($ids | Sort-Object -Unique).Count -ge 3) ($ids -join ',')

    # Elegir un ejercicio suelto apaga el modo mixto.
    Send '{F4}' 1 600
    Send '{DOWN}' 1 150
    Send '{ENTER}' 1 600
    Send '{RIGHT}' 2 350
    $l = Take
    Check 'elegir la recta apaga el modo mixto' (($l -match 'Menú: "recta"').Count -and ($l | Where-Object { $_ -match 'modo mixto' }).Count -eq 0) ($l -join ' | ')

    Send '{F2}' 1 800
    Send '{ESC}' 1 600
    $l = Take
    Check 'F2 abre el panel y Esc lo cierra' (($l -join ' ') -match 'Panel: abierto.*Panel: cerrado') ($l -join ' | ')
}
finally {
    if ($app -and -not $app.HasExited) {
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(10000)) { $app.Kill() }
    }
    if ($backup) { [IO.File]::WriteAllBytes($config, $backup) } else { Remove-Item $config -ErrorAction SilentlyContinue }
}
if ($failures.Count) { Write-Host "$($failures.Count) falla(s)"; exit 1 }
Write-Host 'Prueba de la app real: todo bien'
