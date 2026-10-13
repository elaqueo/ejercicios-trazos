# Prueba de la app real (HU-71): abre Ejercicios (Release), la maneja con teclado como lo haría
# el usuario y verifica en su log lo que pasó. Encuentra lo que los tests de lógica no ven
# (teclas, foco, menús: el cursor del menú que daba la vuelta, HU-20).
#   pwsh scripts/smoke.ps1             # usa build/release/ejercicios.exe y mesadeanimacion.exe (compilarlos antes)
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
        $lines = @(Get-Content $log -Encoding utf8 | Where-Object { $_ -match 'Menú:|Ejercicio:|Repetir:|Guías:|Panel:|Costado:|Punta seca:|Afilada:|Textura del papel:|Vista rotada|Atajos:' })
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

    Send 'i' 2
    $l = Take
    # El costado queda guardado: puede arrancar apagado; dos I lo cambian y lo devuelven.
    Check 'I cambia el costado y lo devuelve' (($l -join ' ') -match 'Costado: (desactivado.*Costado: activado|activado.*Costado: desactivado)') ($l -join ' | ')

    Send 'e' 2
    $l = Take
    Check 'E cambia a la punta seca y vuelve a la mina' (($l -join ' ') -match 'Punta seca: activada.*Punta seca: desactivada') ($l -join ' | ')

    Send 'a'
    $l = Take
    Check 'A afila la mina' ($l -match 'Afilada:').Count ($l -join ' | ')

    Send '^{]}'
    Send '^{[}'
    $l = Take
    Check 'Ctrl+] y Ctrl+[ suben y bajan la textura del papel' (($l -match 'Textura del papel:').Count -eq 2) ($l -join ' | ')

    Send '6'
    Send '4' 2
    Send '5'
    $l = Take
    Check '6, 4 y 5 giran la vista' (($l -join ' ') -match 'Vista rotada \S+ ° \(tecla\).*Vista rotada \S+ ° \(tecla\).*Vista rotada \S+ ° \(tecla\).*Vista rotada 0 ° \(tecla\)') ($l -join ' | ')

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

    Send '^,' 1 800
    Send '{ESC}' 1 600
    Send '^,' 1 1500 # que la ventana de atajos tenga el foco antes de la segunda Ctrl+,
    Send '^,' 1 600
    $l = Take
    Check 'Ctrl+, abre los atajos; Esc y Ctrl+, los cierran' (($l -join ' ') -match 'Atajos: abiertos.*Atajos: cerrados.*Atajos: abiertos.*Atajos: cerrados') ($l -join ' | ')
}
finally {
    if ($app -and -not $app.HasExited) {
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(10000)) { $app.Kill() }
    }
    if ($backup) { [IO.File]::WriteAllBytes($config, $backup) } else { Remove-Item $config -ErrorAction SilentlyContinue }
}
# RNF-01 (HU-90): ningún flip pierde un frame (todos por debajo de 20 ms; uno perdido daría unos
# 33) y como mucho 1 de cada 30 entre 18 y 20 ms: de la tecla al Present va de 0 a un frame según
# cuándo cae, más el render (< 1 ms) y alguna demora de Windows al despertar el hilo (decisión del
# usuario del 12 de octubre de 2026, con HU-122: con 18 estricto, casi cada corrida tenía uno de 18,x).
function FlipsOk([double[]]$flips) {
    $late = @($flips | Where-Object { $_ -ge 18 })
    $lost = @($flips | Where-Object { $_ -ge 20 })
    return $lost.Count -eq 0 -and $late.Count -le [math]::Max(1, [math]::Floor($flips.Count / 30))
}
# Mesa de animación (HU-84): arranca con su nombre y tiene los atajos del lienzo.
$mesa = Join-Path $root 'build\release\mesadeanimacion.exe'
$mesaLog = Join-Path $data 'mesadeanimacion.log'
if (Get-Process mesadeanimacion -ErrorAction SilentlyContinue) { throw 'Mesa de animación está abierta: cerrarla antes de la prueba' }
# Estado conocido: la mesa de luz prendida (HU-91).
$backup = if (Test-Path $config) { [IO.File]::ReadAllBytes($config) } else { $null }
$state = if ($backup) { Get-Content $config -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
if (-not $state.ContainsKey('mesadeanimacion')) { $state['mesadeanimacion'] = @{} }
$state['mesadeanimacion']['lightTable'] = $true
$state['mesadeanimacion']['lightKeysOnly'] = $false # HU-93
$state['mesadeanimacion']['sheetPanel'] = $true # HU-114: el panel de hojas visible
$state['mesadeanimacion']['fieldGuide'] = $false # HU-103: la guía de campo oculta
$state['mesadeanimacion']['timingChart'] = $true # HU-105: la carta de timing visible
$state['mesadeanimacion']['audioStrip'] = $true # HU-106: la franja de audio visible
$state | ConvertTo-Json -Depth 32 | Set-Content $config -Encoding utf8
# La toma de la prueba (HU-85), en una carpeta temporal: la del usuario no se toca.
$takeDir = Join-Path ([IO.Path]::GetTempPath()) "trazos-smoke-toma-$PID"
Remove-Item $takeDir -Recurse -Force -ErrorAction SilentlyContinue
$app = $null
try {
    $app = Start-Process $mesa -ArgumentList '--toma', "`"$takeDir`"" -PassThru
    Start-Sleep 4
    $ws = New-Object -ComObject WScript.Shell
    $null = $ws.AppActivate($app.Id)
    Start-Sleep 1
    $ws.SendKeys('^,'); Start-Sleep -Milliseconds 800
    $ws.SendKeys('{ESC}'); Start-Sleep -Milliseconds 600
    $l = @(Get-Content $mesaLog -Encoding utf8) # el log empieza de cero en cada sesión
    $joined = $l -join ' '
    if ($joined -match 'Mesa de animación' -and $joined -match 'Atajos: abiertos.*Atajos: cerrados') {
        Write-Host '  ok   Mesa de animación arranca y Ctrl+, abre sus atajos'
    } else {
        Write-Host "  FALLA Mesa de animación arranca y Ctrl+, abre sus atajos · $($l -join ' | ')"
        $failures.Add('Mesa de animación')
    }

    # HU-87: Insert agrega, Ctrl+D duplica, Supr borra (en blanco: sin preguntar), Z deshace.
    foreach ($k in '{INSERT}', '^d', '{DEL}', 'z') { $ws.SendKeys($k); Start-Sleep -Milliseconds 700 }
    $l = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Hojas:|Hoja activa' })
    $joined = $l -join ' '
    if ($joined -match 'insertada · hoja 2 de 2.*duplicada · hoja 3 de 3.*borrada · hoja 2 de 2.*Hoja activa 3 de 3') {
        Write-Host '  ok   Insert, Ctrl+D, Supr y Z manejan las hojas'
    } else {
        Write-Host "  FALLA Insert, Ctrl+D, Supr y Z manejan las hojas · $($l -join ' | ')"
        $failures.Add('Hojas')
    }

    # HU-90: 32 flips con ← → por las 3 hojas (cada tecla se aprieta y se suelta: flip y
    # activar); cada uno presentado en el frame siguiente (RNF-01): de la tecla al Present va
    # de 0 a un frame (16,7 ms) según cuándo cae, más la composición (< 1 ms): menos de 18 ms.
    foreach ($n in 1..8) {
        foreach ($k in '{LEFT}', '{LEFT}', '{RIGHT}', '{RIGHT}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 150 }
    }
    Start-Sleep -Milliseconds 400
    $flips = @(Get-Content $mesaLog -Encoding utf8 | ForEach-Object {
        if ($_ -match 'Flip presentado en ([\d.]+) ms') { [double]$Matches[1] } })
    $shown = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Flip: hoja' }).Count
    $slow = @($flips | Where-Object { $_ -ge 18 })
    $max = ($flips | Measure-Object -Maximum).Maximum
    if ($shown -eq 32 -and $flips.Count -ge 30 -and (FlipsOk $flips)) {
        Write-Host "  ok   32 flips con flechas, $($flips.Count) presentados, el más lento en $max ms"
    } else {
        Write-Host "  FALLA flips: $shown pedidos, $($flips.Count) presentados, $($slow.Count) de 18 ms o más (máx. $max ms)"
        $failures.Add('Flip')
    }

    # HU-91: al soltar cada flip la mesa de luz vuelve en menos de 50 ms (RNF-03); L la apaga
    # y la vuelve a prender.
    $lights = @(Get-Content $mesaLog -Encoding utf8 | ForEach-Object {
        if ($_ -match 'Mesa de luz presentada en ([\d.]+) ms') { [double]$Matches[1] } })
    $late = @($lights | Where-Object { $_ -ge 50 })
    $lmax = ($lights | Measure-Object -Maximum).Maximum
    foreach ($k in 'l', 'l') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $toggles = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Mesa de luz: ' }) -join ' ')
    if ($lights.Count -ge 30 -and $late.Count -eq 0 -and $toggles -match 'prendida.*apagada.*prendida') {
        Write-Host "  ok   la mesa de luz vuelve al soltar ($($lights.Count) veces, la más lenta en $lmax ms) y L la apaga y prende"
    } else {
        Write-Host "  FALLA mesa de luz: $($lights.Count) veces, $($late.Count) de 50 ms o más (máx. $lmax ms) · $toggles"
        $failures.Add('Mesa de luz')
    }

    # HU-112: E no hace nada (no hay punta seca; A elige el audio, HU-106).
    foreach ($k in @('e')) { $ws.SendKeys($k); Start-Sleep -Milliseconds 400 }
    $extra = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Afilada|Punta seca' })
    if ($extra.Count -eq 0) {
        Write-Host '  ok   E no hace nada en Mesa de animación'
    } else {
        Write-Host "  FALLA A o E hicieron algo · $($extra -join ' | ')"
        $failures.Add('Medio recortado')
    }

    # HU-99: 2 a 5 eligen los lápices de color y 1 vuelve al grafito; los números ya no giran
    # la vista.
    foreach ($k in '2', '3', '4', '5', '1') { $ws.SendKeys($k); Start-Sleep -Milliseconds 400 }
    $pencils = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Lápiz: |Vista rotada' }) -join ' ')
    if ($pencils -match 'Lápiz: Azul.*Lápiz: Rojo.*Lápiz: Verde.*Lápiz: Violeta.*Lápiz: HB' -and $pencils -notmatch 'Vista rotada') {
        Write-Host '  ok   2 a 5 eligen azul, rojo, verde y violeta, 1 vuelve al grafito'
    } else {
        Write-Host "  FALLA lápices de color · $pencils"
        $failures.Add('Lápices de color')
    }

    # HU-100: F5 abre el selector y 3 elige el rojo (lo cierra); F5 y Esc lo cierran sin
    # cambiar; 1 vuelve al grafito.
    foreach ($k in '{F5}', '3', '{F5}', '{ESC}', '1') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $picker = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Selector de lápices|Lápiz: ' }) -join ' ')
    if ($picker -match 'Selector de lápices: abierto.*Lápiz: Rojo.*Selector de lápices: abierto.*Selector de lápices: cerrado.*Lápiz: HB') {
        Write-Host '  ok   F5 abre el selector, 3 elige el rojo, Esc lo cierra'
    } else {
        Write-Host "  FALLA selector de lápices · $picker"
        $failures.Add('Selector de lápices')
    }

    # HU-116: con un lápiz simulado (scripts/pen.ps1), el botón lateral abre el menú en el centro
    # de la hoja; tocar el rojo lo elige y cierra; la mesa de luz se toca dos veces (el menú queda
    # abierto); Esc cierra. Mantener el botón y soltarlo sobre el violeta lo elige; 1 vuelve al grafito.
    . (Join-Path $PSScriptRoot 'pen.ps1')
    $sheetLine = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'en (\d+) x (\d+) px desde (-?\d+) (-?\d+)' })[0]
    $null = $sheetLine -match 'en (\d+) x (\d+) px desde (-?\d+) (-?\d+)'
    $cx = [int]$Matches[3] + [int]([int]$Matches[1] / 2); $cy = [int]$Matches[4] + [int]([int]$Matches[2] / 2)
    $dpr = [double]((@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'escala de Windows ([\d.]+)' })[0] -replace '.*escala de Windows ([\d.]+).*', '$1'))
    $at = { param([double]$r, [double]$deg) @([int]($cx + $r * $dpr * [math]::Cos($deg * [math]::PI / 180)), [int]($cy - $r * $dpr * [math]::Sin($deg * [math]::PI / 180))) }
    Barrel $cx $cy; Start-Sleep -Milliseconds 400
    $p = & $at 68 (-54); Tap $p[0] $p[1]; Start-Sleep -Milliseconds 400
    Barrel $cx $cy; Start-Sleep -Milliseconds 400
    $p = & $at 140 90; Tap $p[0] $p[1]; Start-Sleep -Milliseconds 400; Tap $p[0] $p[1]; Start-Sleep -Milliseconds 400
    $ws.SendKeys('{ESC}'); Start-Sleep -Milliseconds 500
    Pen $cx $cy; Pen $cx $cy $true 'move' 300
    $p = & $at 68 162; Pen $p[0] $p[1] $true 'move' 150; Pen $p[0] $p[1] $false 'move' 300
    $ws.SendKeys('1'); Start-Sleep -Milliseconds 400
    $menu = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Menú del lápiz|Lápiz: |Mesa de luz: ' }) -join ' ')
    if ($menu -match 'Menú del lápiz: abierto.*Lápiz: Rojo.*Menú del lápiz: abierto.*Mesa de luz: apagada.*Mesa de luz: prendida.*cerrado \(Esc\).*Menú del lápiz: abierto.*Lápiz: Violeta.*Lápiz: HB') {
        Write-Host '  ok   el botón del lápiz abre el menú: el rojo, la mesa de luz dos veces, Esc, y soltar sobre el violeta'
    } else {
        Write-Host "  FALLA menú del lápiz · $menu"
        $failures.Add('Menú del lápiz')
    }

    # HU-104: K marca la activa como key y queda en toma.json (en la pausa siguiente); K K la
    # pasan a breakdown y otra vez a sin rol.
    $ws.SendKeys('k'); Start-Sleep -Milliseconds 2500
    $json = Get-Content (Join-Path $takeDir 'toma.json') -Raw -Encoding utf8 | ConvertFrom-Json
    $keyed = @($json.hojas | Where-Object { $_.rol -eq 'key' }).Count
    foreach ($k in 'k', 'k') { $ws.SendKeys($k); Start-Sleep -Milliseconds 400 }
    $roles = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Rol: ' }) -join ' ')
    if ($keyed -eq 1 -and $roles -match 'Rol: key.*Rol: breakdown.*Rol: sin rol') {
        Write-Host '  ok   K marca key (guardado en toma.json), breakdown y sin rol'
    } else {
        Write-Host "  FALLA rol: $keyed key en toma.json · $roles"
        $failures.Add('Rol')
    }

    # HU-93: Ctrl+L sin keys avisa que no hay; Ctrl+L vuelve a todas; con la 1 como key, desde
    # la 3 Ctrl+L la muestra sola. Después la 1 vuelve a sin rol y la activa a la 3 de 3.
    # Ctrl con keybd_event, mantenido un momento: el ^ de SendKeys a veces llega suelto.
    Add-Type -Namespace Smoke -Name Keys -MemberDefinition '[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, System.UIntPtr extra);'
    function Ctrl([byte]$vk) {
        [Smoke.Keys]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
        [Smoke.Keys]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        [Smoke.Keys]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
        [Smoke.Keys]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 400
    }
    function CtrlL { Ctrl 0x4C }
    function Key([byte]$vk) {
        [Smoke.Keys]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        [Smoke.Keys]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 400
    }
    CtrlL; CtrlL
    foreach ($k in '{HOME}', 'k', '{END}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 450 }
    CtrlL; CtrlL
    foreach ($k in '{HOME}', 'k', 'k', '{END}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 450 }
    $keysOnly = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Mesa de luz: (prendida|apagada|no hay)' }) -join ' ')
    if ($keysOnly -match 'no hay keys alrededor de la hoja 3.*prendida \(solo keys\).*Mesa de luz: prendida .*Mesa de luz: prendida \(solo keys\) .*Mesa de luz: prendida$' -and
        ([regex]::Matches($keysOnly, 'no hay keys')).Count -eq 1) {
        Write-Host '  ok   Ctrl+L muestra solo las keys (sin keys, avisa) y vuelve a todas'
    } else {
        Write-Host "  FALLA solo keys · $keysOnly"
        $failures.Add('Solo keys')
    }

    # HU-101: + + − (las del teclado numérico) acercan y alejan de a 20 %; Ctrl+1 vuelve a 100 %;
    # + y Ctrl+0 vuelven a la vista inicial.
    Key 0x6B; Key 0x6B; Key 0x6D # VK_ADD, VK_SUBTRACT
    Ctrl 0x31
    Key 0x6B
    Ctrl 0x30
    $zoom = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Zoom |Vista inicial' }) -join ' ')
    if ($zoom -match 'Zoom 120 % \(\+\).*Zoom 144 % \(\+\).*Zoom 120 % \(−\).*Zoom 100 % \(Ctrl\+1\).*Zoom 120 % \(\+\).*Vista inicial: rotada 0 °, zoom 100 %') {
        Write-Host '  ok   + y − acercan y alejan, Ctrl+1 vuelve a 100 % y Ctrl+0 a la vista inicial'
    } else {
        Write-Host "  FALLA zoom · $zoom"
        $failures.Add('Zoom')
    }

    # HU-102: con Espacio apretado, el lápiz simulado arrastra (200, 100) px: la vista se corre
    # eso y no se dibuja (Supr borra la hoja sin preguntar, porque sigue en blanco; Z la devuelve).
    # Ctrl+1 la vuelve a centrar.
    [Smoke.Keys]::keybd_event(0x20, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 150
    Pen $cx $cy; Pen $cx $cy $false 'down' 60
    foreach ($s in 1..10) { [SynthPen]::Send($cx + 20 * $s, $cy + 10 * $s, $true, $false, 'move'); Start-Sleep -Milliseconds 20 }
    Pen ($cx + 200) ($cy + 100) $false 'up' 80
    [Smoke.Keys]::keybd_event(0x20, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 300
    Key 0x2E; Start-Sleep -Milliseconds 300 # Supr
    $ws.SendKeys('z'); Start-Sleep -Milliseconds 500
    Ctrl 0x31
    $pan = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Vista corrida|Hojas: borrada|Zoom 100 % \(Ctrl\+1\)|Hoja activa' }) | Select-Object -Last 6) -join ' '
    if ($pan -match 'Vista corrida a \(200, 100\) px.*Hojas: borrada · hoja 2 de 2.*Hoja activa 3 de 3.*Zoom 100 % \(Ctrl\+1\)') {
        Write-Host '  ok   Espacio + arrastrar corre la vista sin dibujar y Ctrl+1 la centra'
    } else {
        Write-Host "  FALLA pan · $pan"
        $failures.Add('Pan')
    }

    # HU-88: con la activa en la 3 de 3, ↑ ↑ la llevan al principio, otra ↑ no hace nada, ↓ la
    # vuelve a la 2 y Z deshace eso: queda primera, y así se guarda en toma.json.
    foreach ($k in '{UP}', '{UP}', '{UP}', '{DOWN}', 'z') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    Start-Sleep -Milliseconds 700
    $moves = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'movida|extremo' }) -join ' ')
    $json = Get-Content (Join-Path $takeDir 'toma.json') -Raw -Encoding utf8 | ConvertFrom-Json
    if ($moves -match 'principio · hoja 2 de 3.*principio · hoja 1 de 3.*extremo.*final · hoja 2 de 3' -and $json.activa -eq 0) {
        Write-Host '  ok   ↑ ↓ mueven la hoja, en el extremo se queda y Z lo deshace (guardado en toma.json)'
    } else {
        Write-Host "  FALLA ↑ ↓: $moves · activa en toma.json: $($json.activa)"
        $failures.Add('Reordenar')
    }

    # HU-115: con la activa en la 1 de 3, Fin salta a la última, Fin otra vez no hace nada e
    # Inicio vuelve a la primera.
    foreach ($k in '{END}', '{END}', '{HOME}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $jumps = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'la última|la primera' }) -join ' ')
    if ($jumps -match 'la última · hoja 3 de 3.*ya está en la última.*la primera · hoja 1 de 3') {
        Write-Host '  ok   Fin salta a la última hoja e Inicio a la primera'
    } else {
        Write-Host "  FALLA Inicio y Fin: $jumps"
        $failures.Add('Inicio y Fin')
    }

    # HU-89: F deja fija la 1 de 3 (queda en toma.json) y se sigue en la vecina: Fin e Inicio la
    # saltean (hoja 2 de 2 y 1 de 2); Ctrl+F entra a dibujarla y vuelve; Ctrl+F y F la desmarcan
    # y la pila vuelve a ser de 3, con la activa en la 1.
    $ws.SendKeys('f'); Start-Sleep -Milliseconds 2500
    $json = Get-Content (Join-Path $takeDir 'toma.json') -Raw -Encoding utf8 | ConvertFrom-Json
    $fixedCount = @($json.hojas | Where-Object { $_.fija }).Count
    foreach ($k in '{END}', '{HOME}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    Ctrl 0x46; Ctrl 0x46; Ctrl 0x46
    $ws.SendKeys('f'); Start-Sleep -Milliseconds 500
    $fixedLog = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Hoja fija|la última|la primera' }) | Select-Object -Last 7) -join ' '
    if ($fixedCount -eq 1 -and $fixedLog -match 'Hoja fija: la activa.*la última · hoja 2 de 2.*la primera · hoja 1 de 2.*dibujando en ella.*vuelve a la hoja 1 de 2.*dibujando en ella.*ninguna \(vuelve al flip como hoja 1 de 3\)') {
        Write-Host '  ok   F deja fija la hoja (en toma.json), Inicio y Fin la saltean, Ctrl+F entra y vuelve, F la desmarca'
    } else {
        Write-Host "  FALLA hoja fija: $fixedCount fija en toma.json · $fixedLog"
        $failures.Add('Hoja fija')
    }

    # HU-94: con la activa en la 1 de 3, O prende el círculo y ← va a la última; → vuelve a la
    # primera y O lo apaga.
    $ws.SendKeys('o'); Start-Sleep -Milliseconds 400
    foreach ($k in '{LEFT}', '{RIGHT}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $ws.SendKeys('o'); Start-Sleep -Milliseconds 400
    $circle = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Pila en círculo|Flip: activa' }) | Select-Object -Last 4) -join ' '
    if ($circle -match 'círculo: sí.*activa la hoja 3 de 3.*activa la hoja 1 de 3.*círculo: no') {
        Write-Host '  ok   O pone la pila en círculo: ← desde la primera va a la última y → vuelve'
    } else {
        Write-Host "  FALLA pila en círculo: $circle"
        $failures.Add('Pila en círculo')
    }

    # HU-107: sin audio, P avisa que no hay (la reproducción la prueba tst_audio).
    $ws.SendKeys('p'); Start-Sleep -Milliseconds 400
    $play = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Audio: ' }) | Select-Object -Last 1) -join ' '
    if ($play -match 'no hay audio') {
        Write-Host '  ok   P sin audio avisa que no hay'
    } else {
        Write-Host "  FALLA play sin audio: $play"
        $failures.Add('Play')
    }

    # HU-108: sin audio, M avisa que no hay y Ctrl+M que no hay marcas (las marcas las prueban
    # tst_lienzo y la prueba con audio).
    $ws.SendKeys('m'); Start-Sleep -Milliseconds 400
    Ctrl 0x4D
    $marks = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Audio: ' }) | Select-Object -Last 2) -join ' '
    if ($marks -match 'no hay audio.*no hay marcas') {
        Write-Host '  ok   M y Ctrl+M sin audio avisan'
    } else {
        Write-Host "  FALLA marcas sin audio: $marks"
        $failures.Add('Marcas')
    }

    # HU-106: W oculta la franja de audio y la vuelve a mostrar.
    foreach ($k in 'w', 'w') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $strip = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Franja de audio:' }) | Select-Object -Last 2) -join ' '
    if ($strip -match 'Franja de audio: oculta.*Franja de audio: visible') {
        Write-Host '  ok   W oculta y muestra la franja de audio'
    } else {
        Write-Host "  FALLA franja de audio: $strip"
        $failures.Add('Franja de audio')
    }

    # HU-103: G muestra la guía de campo y la oculta.
    foreach ($k in 'g', 'g') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $guide = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Guía de campo:' }) | Select-Object -Last 2) -join ' '
    if ($guide -match 'Guía de campo: visible.*Guía de campo: oculta') {
        Write-Host '  ok   G muestra y oculta la guía de campo'
    } else {
        Write-Host "  FALLA guía de campo: $guide"
        $failures.Add('Guía de campo')
    }

    # HU-105: C oculta la carta de timing y la vuelve a mostrar.
    foreach ($k in 'c', 'c') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $chart = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Carta de timing:' }) | Select-Object -Last 2) -join ' '
    if ($chart -match 'Carta de timing: oculta.*Carta de timing: visible en las keys') {
        Write-Host '  ok   C oculta y muestra la carta de timing'
    } else {
        Write-Host "  FALLA carta de timing: $chart"
        $failures.Add('Carta de timing')
    }

    # HU-114: Tab oculta el panel de hojas y lo vuelve a mostrar (los flips de arriba ya
    # corrieron con el panel visible).
    foreach ($k in '{TAB}', '{TAB}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $panel = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Panel de hojas:' }) -join ' ')
    if ($panel -match 'oculto.*visible') {
        Write-Host '  ok   Tab oculta y muestra el panel de hojas'
    } else {
        Write-Host "  FALLA panel de hojas: $panel"
        $failures.Add('Panel de hojas')
    }

    # HU-109: Ctrl+E exporta la toma a <toma>/export: un PNG por hoja del flip y toma.json.
    Ctrl 0x45
    $exported = ''
    foreach ($i in 1..40) {
        $exported = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Exportar: (\d+ hojas en|no se pudo)' }) | Select-Object -Last 1) -join ' '
        if ($exported) { break }
        Start-Sleep -Milliseconds 500
    }
    $pngs = @(Get-ChildItem (Join-Path $takeDir 'export') -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^\d{4}\.png$' }).Count
    if ($exported -match 'Exportar: (\d+) hojas en' -and [int]$Matches[1] -eq $pngs -and $pngs -ge 1 -and
        (Test-Path (Join-Path $takeDir 'export\toma.json'))) {
        Write-Host "  ok   Ctrl+E exporta $pngs PNG y toma.json"
    } else {
        Write-Host "  FALLA exportar: $exported · $pngs PNG"
        $failures.Add('Exportar')
    }

    # HU-85 y HU-86: la toma (toma.json y una hoja por archivo) se guarda sola, sin cerrar: las 3
    # hojas ya están en disco, e Insert agrega el archivo de la nueva enseguida. Con la app
    # matada (sin cerrar), al volver a abrir está la misma pila, con la misma hoja activa, sin
    # redibujar ni hojas en blanco.
    $sheetFiles = { @(Get-ChildItem $takeDir -Filter 'hoja-*.hoja' -ErrorAction SilentlyContinue).Count }
    Start-Sleep -Milliseconds 1000
    $before = & $sheetFiles
    $ws.SendKeys('{INSERT}')
    Start-Sleep -Milliseconds 1500
    $after = & $sheetFiles
    $inserted = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'insertada · hoja (\d+) de 4' }) -join ' ')
    $expected = if ($inserted -match 'hoja (\d+) de 4') { "4 hojas, activa la $($Matches[1]), 0 redibujadas, 0 en blanco" } else { '(no se insertó)' }
    $app.Kill()
    $null = $app.WaitForExit(5000)
    $app = Start-Process $mesa -ArgumentList '--toma', "`"$takeDir`"" -PassThru
    Start-Sleep 5
    $opened = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Toma abierta de' }) -join ' ')
    if ($before -eq 3 -and $after -eq 4 -and (Test-Path (Join-Path $takeDir 'toma.json')) -and $opened.Contains($expected)) {
        Write-Host "  ok   la toma se guarda sola (3 hojas, Insert: 4) y abre igual después de matar la app ($expected)"
    } else {
        Write-Host "  FALLA toma: $before y $after archivos de hoja · esperaba $expected · $opened"
        $failures.Add('Toma')
    }

    # HU-86: cerrar espera solo lo pendiente (menos de 2 s, RNF) y la toma vuelve a abrir igual.
    # Se cierra con Ctrl+Q (pedido del usuario con HU-109).
    $null = $ws.AppActivate($app.Id); Start-Sleep -Milliseconds 500
    Ctrl 0x51
    if (-not $app.WaitForExit(15000)) {
        Write-Host '  FALLA Ctrl+Q no cerró la app'
        $failures.Add('Ctrl+Q')
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(15000)) { $app.Kill() }
    }
    $closed = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Toma: guardada al cerrar en' }) -join ' ')
    $closeMs = if ($closed -match 'en (\d+) ms') { [int]$Matches[1] } else { -1 }
    $app = Start-Process $mesa -ArgumentList '--toma', "`"$takeDir`"" -PassThru
    Start-Sleep 5
    $opened = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Toma abierta de' }) -join ' ')
    if ($closeMs -ge 0 -and $closeMs -lt 2000 -and $opened.Contains($expected)) {
        Write-Host "  ok   Ctrl+Q cierra: espera $closeMs ms y la toma abre igual"
    } else {
        Write-Host "  FALLA cierre: $closed · $opened"
        $failures.Add('Cierre')
    }
}
finally {
    if ($app -and -not $app.HasExited) {
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(15000)) { $app.Kill() }
    }
    if ($backup) { [IO.File]::WriteAllBytes($config, $backup) } else { Remove-Item $config -ErrorAction SilentlyContinue }
    Remove-Item $takeDir -Recurse -Force -ErrorAction SilentlyContinue
}
# HU-119: el flip con una guía en cada hoja sigue en RNF-01 (12 hojas, --guias-de-prueba).
$backup = if (Test-Path $config) { [IO.File]::ReadAllBytes($config) } else { $null }
$state = if ($backup) { Get-Content $config -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
if (-not $state.ContainsKey('mesadeanimacion')) { $state['mesadeanimacion'] = @{} }
$state['mesadeanimacion']['lightTable'] = $true
$state['mesadeanimacion']['lightKeysOnly'] = $false
$state['mesadeanimacion']['fieldGuide'] = $false
$state | ConvertTo-Json -Depth 32 | Set-Content $config -Encoding utf8
$takeDir = Join-Path ([IO.Path]::GetTempPath()) "trazos-smoke-guias-$PID"
Remove-Item $takeDir -Recurse -Force -ErrorAction SilentlyContinue
$app = $null
try {
    $app = Start-Process $mesa -ArgumentList '--toma', "`"$takeDir`"", '--guias-de-prueba' -PassThru
    Start-Sleep 4
    $ws = New-Object -ComObject WScript.Shell
    $null = $ws.AppActivate($app.Id)
    Start-Sleep 1
    foreach ($n in 1..11) { $ws.SendKeys('{INSERT}'); Start-Sleep -Milliseconds 300 }
    Start-Sleep -Milliseconds 500
    $start = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Guías de prueba en las' }).Count
    $before = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Flip: hoja' }).Count
    foreach ($n in 1..2) {
        foreach ($k in (@('{LEFT}') * 11 + @('{RIGHT}') * 11)) { $ws.SendKeys($k); Start-Sleep -Milliseconds 150 }
    }
    Start-Sleep -Milliseconds 400
    $flips = @(Get-Content $mesaLog -Encoding utf8 | ForEach-Object {
        if ($_ -match 'Flip presentado en ([\d.]+) ms') { [double]$Matches[1] } })
    $shown = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Flip: hoja' }).Count - $before
    $sheets = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'hoja \d+ de 12' }).Count
    $slow = @($flips | Where-Object { $_ -ge 18 })
    $max = ($flips | Measure-Object -Maximum).Maximum
    $lights = @(Get-Content $mesaLog -Encoding utf8 | ForEach-Object {
        if ($_ -match 'Mesa de luz presentada en ([\d.]+) ms') { [double]$Matches[1] } })
    $late = @($lights | Where-Object { $_ -ge 50 })
    $lmax = ($lights | Measure-Object -Maximum).Maximum
    if ($start -eq 1 -and $sheets -gt 0 -and $shown -eq 44 -and $flips.Count -ge 40 -and (FlipsOk $flips) -and $late.Count -eq 0) {
        Write-Host "  ok   con guía en las 12 hojas: 44 flips, el más lento en $max ms; la mesa de luz en $lmax ms"
    } else {
        Write-Host "  FALLA flip con guías: arranque $start, $shown pedidos, $($flips.Count) presentados, $($slow.Count) de 18 ms o más (máx. $max ms), mesa de luz máx. $lmax ms"
        $failures.Add('Flip con guías')
    }
}
finally {
    if ($app -and -not $app.HasExited) {
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(15000)) { $app.Kill() }
    }
    if ($backup) { [IO.File]::WriteAllBytes($config, $backup) } else { Remove-Item $config -ErrorAction SilentlyContinue }
    Remove-Item $takeDir -Recurse -Force -ErrorAction SilentlyContinue
}
# Ejercicios de animación (HU-120, HU-121, HU-123, HU-124): arranca con su nombre y sus atajos en el último
# ejercicio (aquí el prisma que gira en 12 hojas); Enter, R y G arman la pila en menos de 300 ms (RNF-04);
# el flip y la mesa de luz como en la Mesa.
$anim = Join-Path $root 'build\release\ejerciciosanimacion.exe'
$animLog = Join-Path $data 'ejerciciosanimacion.log'
if (Get-Process ejerciciosanimacion -ErrorAction SilentlyContinue) { throw 'Ejercicios de animación está abierta: cerrarla antes de la prueba' }
$backup = if (Test-Path $config) { [IO.File]::ReadAllBytes($config) } else { $null }
$state = if ($backup) { Get-Content $config -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
if (-not $state.ContainsKey('ejerciciosanimacion')) { $state['ejerciciosanimacion'] = @{} }
$state['ejerciciosanimacion']['lightTable'] = $true
$state['ejerciciosanimacion']['exercise'] = 'prismaQueGira'
$state['ejerciciosanimacion']['mixed'] = $false
$state['ejerciciosanimacion']['params'] = @{ prismaQueGira = @{ hojas = 12 } }
$state | ConvertTo-Json -Depth 32 | Set-Content $config -Encoding utf8
$app = $null
try {
    $app = Start-Process $anim -PassThru
    Start-Sleep 4
    $ws = New-Object -ComObject WScript.Shell
    $null = $ws.AppActivate($app.Id)
    Start-Sleep 1
    $ws.SendKeys('^,'); Start-Sleep -Milliseconds 800
    $ws.SendKeys('{ESC}'); Start-Sleep -Milliseconds 600
    $joined = @(Get-Content $animLog -Encoding utf8) -join ' '
    if ($joined -match 'Ejercicios de animación' -and $joined -match 'Pila armada: 12 hojas' -and
        $joined -match 'Atajos: abiertos.*Atajos: cerrados') {
        Write-Host '  ok   Ejercicios de animación arranca en el prisma que gira con 12 hojas y Ctrl+, abre sus atajos'
    } else {
        Write-Host "  FALLA Ejercicios de animación arranca · $joined"
        $failures.Add('Ejercicios de animación')
    }

    # HU-121: Enter arma otra pila, R la misma y G oculta y muestra las guías; armar tarda menos
    # de 300 ms (RNF-04).
    foreach ($k in '{ENTER}', 'r', 'g', 'g') { $ws.SendKeys($k); Start-Sleep -Milliseconds 600 }
    $l = @(Get-Content $animLog -Encoding utf8)
    $builds = @($l | ForEach-Object { if ($_ -match 'Pila lista: 12 hojas con sus guías en (\d+) ms') { [int]$Matches[1] } })
    $joined = $l -join ' '
    $bmax = ($builds | Measure-Object -Maximum).Maximum
    if ($builds.Count -eq 2 -and $bmax -lt 300 -and $joined -match 'Ejercicio: "prismaQueGira"' -and
        $joined -match 'Repetir: "prismaQueGira"' -and $joined -match 'Guías de las hojas: ocultas.*Guías de las hojas: visibles') {
        Write-Host "  ok   Enter y R arman la pila de 12 hojas (la más lenta en $bmax ms) y G oculta y muestra las guías"
    } else {
        Write-Host "  FALLA Enter, R y G: $($builds.Count) pilas armadas (máx. $bmax ms) · $joined"
        $failures.Add('Sesión de pila')
    }

    foreach ($k in (@('{RIGHT}') * 11 + @('{LEFT}') * 11)) { $ws.SendKeys($k); Start-Sleep -Milliseconds 150 }
    Start-Sleep -Milliseconds 400
    $flips = @(Get-Content $animLog -Encoding utf8 | ForEach-Object {
        if ($_ -match 'Flip presentado en ([\d.]+) ms') { [double]$Matches[1] } })
    $shown = @(Get-Content $animLog -Encoding utf8 | Where-Object { $_ -match 'Flip: hoja' }).Count
    $slow = @($flips | Where-Object { $_ -ge 18 })
    $max = ($flips | Measure-Object -Maximum).Maximum
    foreach ($k in 'l', 'l') { $ws.SendKeys($k); Start-Sleep -Milliseconds 500 }
    $toggles = (@(Get-Content $animLog -Encoding utf8 | Where-Object { $_ -match 'Mesa de luz: ' }) -join ' ')
    if ($shown -eq 22 -and $flips.Count -ge 20 -and (FlipsOk $flips) -and $toggles -match 'prendida.*apagada.*prendida') {
        Write-Host "  ok   22 flips por las 12 hojas, el más lento en $max ms, y L apaga y prende la mesa de luz"
    } else {
        Write-Host "  FALLA flip en Ejercicios de animación: $shown pedidos, $($flips.Count) presentados, $($slow.Count) de 18 ms o más (máx. $max ms) · $toggles"
        $failures.Add('Flip en Ejercicios de animación')
    }

    # HU-124: F4 abre el menú y elegir otro ejercicio (el de abajo del prisma en la grilla) arma su
    # pila (6 hojas por defecto los trazos, 12 las formas que giran).
    foreach ($k in '{F4}', '{DOWN}', '{ENTER}') { $ws.SendKeys($k); Start-Sleep -Milliseconds 600 }
    $l = @(Get-Content $animLog -Encoding utf8)
    $picked = @($l | ForEach-Object { if ($_ -match 'Menú: "([^"]+)"') { $Matches[1] } })
    $last = $l | Where-Object { $_ -match 'Pila armada: (\d+) hojas' } | Select-Object -Last 1
    if ($picked.Count -eq 1 -and $picked[0] -ne 'prismaQueGira' -and $last -match 'Pila armada: (6|12) hojas') {
        Write-Host "  ok   F4 elige otro ejercicio ($($picked[0])) y arma su pila de $($Matches[1]) hojas"
    } else {
        Write-Host "  FALLA F4 en Ejercicios de animación: elegido $($picked -join ',') · $last"
        $failures.Add('Menú de Ejercicios de animación')
    }
    Ctrl 0x51
    if ($app.WaitForExit(10000)) {
        Write-Host '  ok   Ctrl+Q cierra Ejercicios de animación'
    } else {
        Write-Host '  FALLA Ctrl+Q no cerró Ejercicios de animación'
        $failures.Add('Cierre de Ejercicios de animación')
    }
}
finally {
    if ($app -and -not $app.HasExited) {
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(15000)) { $app.Kill() }
    }
    if ($backup) { [IO.File]::WriteAllBytes($config, $backup) } else { Remove-Item $config -ErrorAction SilentlyContinue }
}
# Cartuchera en modo cuaderno (HU-133, Bocetos): un cuaderno de 3 hojas en una carpeta temporal.
# --hoja abre la 1 con el papel del cuaderno; Ctrl+Enter en blanco pregunta y Esc sigue; un trazo y
# Ctrl+Q la dejan en borrador; al volver a abrirla el trazo sigue (Ctrl+Enter ya no pregunta) y la
# termina: terminada.png y la fecha del cuaderno. Una hoja terminada no se vuelve a abrir.
$cart = Join-Path $root 'build\release\cartuchera.exe'
$cartLog = Join-Path $data 'cartuchera.log'
if (Get-Process cartuchera -ErrorAction SilentlyContinue) { throw 'Cartuchera está abierta: cerrarla antes de la prueba' }
$nbDir = Join-Path ([IO.Path]::GetTempPath()) "trazos-smoke-cuaderno-$PID"
Remove-Item $nbDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $nbDir | Out-Null
$nbJson = Join-Path $nbDir 'cuaderno.json'
@{ version = 1; name = 'Prueba'; collection = ''; cover = '#a9533d'; maxSheets = 3; started = '2026-10-01'; updated = '2026-10-01'
   paper = @{ preset = 'kraft'; color = '#b59878'; grain = 2047; texture = 35 } } | ConvertTo-Json -Depth 4 | Set-Content $nbJson -Encoding utf8
$sheet1 = Join-Path $nbDir 'hoja-0001'
$app = $null
try {
    if (-not ('Smoke.Keys' -as [type])) {
        Add-Type -Namespace Smoke -Name Keys -MemberDefinition '[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, System.UIntPtr extra);'
    }
    if (-not ('SynthPen' -as [type])) { . (Join-Path $PSScriptRoot 'pen.ps1') }
    function NbCtrl([byte]$vk) {
        [Smoke.Keys]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
        [Smoke.Keys]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        [Smoke.Keys]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
        [Smoke.Keys]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 600
    }
    function NbKey([byte]$vk) {
        [Smoke.Keys]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        [Smoke.Keys]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 600
    }
    function NbOpen {
        $p = Start-Process $cart -ArgumentList '--hoja', "`"$sheet1`"" -PassThru
        Start-Sleep 4
        $null = (New-Object -ComObject WScript.Shell).AppActivate($p.Id)
        Start-Sleep 1
        return $p
    }

    $app = NbOpen
    NbCtrl 0x0D # Ctrl+Enter, en blanco: pregunta
    NbKey 0x1B  # Esc: sigue dibujando
    $l = @(Get-Content $cartLog -Encoding utf8) -join ' '
    Check 'Cartuchera abre la hoja 1 del cuaderno con su papel; en blanco Ctrl+Enter pregunta y Esc sigue' `
        ($l -match 'Cuaderno: Prueba · hoja 1 de 3 · papel kraft' -and $l -match 'confirmar terminar en blanco.*terminar cancelado') $l

    $null = $l -match 'en (\d+) x (\d+) px desde (-?\d+) (-?\d+)'
    $cx = [int]$Matches[3] + [int]([int]$Matches[1] / 2); $cy = [int]$Matches[4] + [int]([int]$Matches[2] / 2)
    Pen $cx $cy; Pen $cx $cy $false 'down' 60
    foreach ($s in 1..12) { [SynthPen]::Send($cx + 20 * $s, $cy + 8 * $s, $true, $false, 'move'); Start-Sleep -Milliseconds 20 }
    Pen ($cx + 240) ($cy + 96) $false 'up' 80
    Start-Sleep 2
    NbCtrl 0x51 # Ctrl+Q: queda el borrador
    $closed = $app.WaitForExit(15000)
    $draft = (Test-Path (Join-Path $sheet1 'toma.json')) -and -not (Test-Path (Join-Path $sheet1 'terminada.png'))
    Check 'un trazo y Ctrl+Q dejan la hoja en borrador' ($closed -and $draft) "cerró: $closed · borrador: $draft"

    $app = NbOpen
    NbCtrl 0x0D # con el trazo: termina sin preguntar
    $closed = $app.WaitForExit(15000)
    $l = @(Get-Content $cartLog -Encoding utf8) -join ' '
    $updated = (Get-Content $nbJson -Raw | ConvertFrom-Json).updated
    $today = (Get-Date).ToString('yyyy-MM-dd')
    Check 'al volver el trazo sigue y Ctrl+Enter la termina: terminada.png y la fecha del cuaderno' `
        ($closed -and (Test-Path (Join-Path $sheet1 'terminada.png')) -and $updated -eq $today -and $l -match 'hoja 1 terminada' -and $l -notmatch 'confirmar terminar') `
        "cerró: $closed · fecha: $updated · $l"

    $p = Start-Process $cart -ArgumentList '--hoja', "`"$sheet1`"" -PassThru
    $exited = $p.WaitForExit(10000)
    $l = @(Get-Content $cartLog -Encoding utf8) -join ' '
    Check 'una hoja terminada no se vuelve a abrir' ($exited -and $l -match 'no se abre: la hoja 1 ya está terminada') $l
    $app = $null
}
finally {
    if ($app -and -not $app.HasExited) {
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(15000)) { $app.Kill() }
    }
    Remove-Item $nbDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Bocetos (HU-134, HU-135): una biblioteca temporal de 30 cuadernos en 3 estantes (--biblioteca) se lee
# en menos de 500 ms (RNF-01); encuentra Cartuchera al lado; Ctrl+, abre los atajos y Esc los cierra;
# las flechas eligen otro cuaderno y Enter lo abre; Ctrl+Q sale. HU-137: cada cuaderno tiene de 2 a 4
# hojas terminadas de tamaño real (7680 × 4800, como las de Cartuchera); AvPág y RePág pasan de hoja
# en menos de 100 ms (RNF-02), + y − acercan y alejan, Esc vuelve a la biblioteca. HU-138: D dibuja la
# siguiente hoja en Cartuchera; F11 pantalla completa. HU-145: con --tv apuntando a un TV de mentira
# (fake_renderer, nunca el de verdad), T manda la hoja, AvPág la siguiente y T corta. HU-142: otra
# biblioteca con un cuaderno de Krita y el Krita de mentira (--krita fake_krita, nunca el de verdad): D
# arma el papel con Cartuchera, "dibuja" y guarda en Krita, Bocetos muestra el borrador y Ctrl+Enter,
# Ctrl+Enter lo terminan. HU-143: el panel de la biblioteca arma las miniaturas del elegido. HU-140 y
# HU-141: con --exportar-en (sin el diálogo de Windows), Ctrl+E guarda la hoja como PNG y Ctrl+P el
# cuaderno como PDF (la tapa y la hoja: 2 páginas).
$boc = Join-Path $root 'build\release\bocetos.exe'
$bocLog = Join-Path $data 'bocetos.log'
if (Get-Process bocetos -ErrorAction SilentlyContinue) { throw 'Bocetos está abierta: cerrarla antes de la prueba' }
if (Get-Process cartuchera -ErrorAction SilentlyContinue) { throw 'Cartuchera está abierta: cerrarla antes de la prueba' }
$bocBackup = if (Test-Path $config) { [IO.File]::ReadAllBytes($config) } else { $null } # guarda el elegido
$libDir = Join-Path ([IO.Path]::GetTempPath()) "trazos-smoke-bocetos-$PID"
Remove-Item $libDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $libDir | Out-Null
Add-Type -AssemblyName System.Drawing
$sheetPng = Join-Path $libDir 'hoja.png' # una hoja de verdad, copiada a todas
$bmp = New-Object System.Drawing.Bitmap 7680, 4800, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.Clear([System.Drawing.Color]::FromArgb(0xF5, 0xF0, 0xE6))
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(0x2A, 0x2A, 0x2A)), 12
foreach ($k in 0..40) { $gfx.DrawLine($pen, 400 + $k * 160, 600, 1200 + $k * 140, 4200) }
$gfx.Dispose(); $bmp.Save($sheetPng, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
foreach ($i in 1..30) {
    $dir = Join-Path $libDir "cuaderno-$i"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $coll = @('', 'Proyecto xxx', 'Estudios')[$i % 3]
    @{ version = 1; name = "Cuaderno $i"; collection = $coll; cover = '#3f5f80'; maxSheets = 40; started = '2026-10-01'
       updated = ('2026-10-{0:d2}' -f (1 + $i % 28)); paper = @{ preset = 'crema'; color = '#f5f0e6'; grain = 2047; texture = 20 } } |
        ConvertTo-Json -Depth 4 | Set-Content (Join-Path $dir 'cuaderno.json') -Encoding utf8
    foreach ($h in 1..(2 + $i % 3)) { # de 2 a 4 hojas terminadas
        $sheet = Join-Path $dir ('hoja-{0:d4}' -f $h)
        New-Item -ItemType Directory -Force $sheet | Out-Null
        Copy-Item $sheetPng (Join-Path $sheet 'terminada.png')
    }
}
$app = $null
$tvLog = Join-Path ([IO.Path]::GetTempPath()) "trazos-smoke-tv-$PID.log"
$tvProc = Start-Process (Join-Path $root 'build\release\fake_renderer.exe') -ArgumentList '--port', '8799', '--log', "`"$tvLog`"" -PassThru -WindowStyle Hidden
try {
    if (-not ('Smoke.Keys' -as [type])) {
        Add-Type -Namespace Smoke -Name Keys -MemberDefinition '[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, System.UIntPtr extra);'
    }
    function BoCtrl([byte]$vk) {
        [Smoke.Keys]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
        [Smoke.Keys]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        [Smoke.Keys]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
        [Smoke.Keys]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 600
    }
    function BoKey([byte]$vk) {
        [Smoke.Keys]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        [Smoke.Keys]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 400
    }
    $app = Start-Process $boc -ArgumentList '--biblioteca', "`"$libDir`"", '--tv', 'http://127.0.0.1:8799/dmr' -PassThru
    Start-Sleep 3
    $null = (New-Object -ComObject WScript.Shell).AppActivate($app.Id)
    Start-Sleep 1
    BoCtrl 0xBC # Ctrl+,
    BoKey 0x1B  # Esc
    $l = @(Get-Content $bocLog -Encoding utf8) -join ' '
    $ms = if ($l -match 'Biblioteca: 30 cuadernos en 3 estantes, leída en ([\d.]+) ms') { [double]$Matches[1] } else { -1 }
    Check "Bocetos lee 30 cuadernos en 3 estantes en menos de 500 ms ($ms ms), encuentra Cartuchera y Ctrl+, abre sus atajos" `
        ($ms -ge 0 -and $ms -lt 500 -and $l -match 'Bocetos: Cartuchera en' -and $l -match 'Atajos: abiertos.*Atajos: cerrados') $l
    BoKey 0x27; BoKey 0x27; BoKey 0x25; BoKey 0x28 # → → ← ↓: cuatro cambios, con cualquier ancho (el general tiene 10)
    # HU-143: el panel arma las miniaturas del elegido (miniatura.png junto a cada hoja); al volver a
    # uno ya visto salen de la memoria.
    Start-Sleep 3; BoKey 0x27; Start-Sleep 3; BoKey 0x25; BoKey 0x27 # → (nuevo) ← → (ya vistos)
    $minis = @(Get-Content $bocLog -Encoding utf8 | Where-Object { $_ -match 'Miniaturas: «.*» · \d+ listas en ([\d.]+) ms' } | ForEach-Object { [double]($_ -replace '.* listas en ([\d.]+) ms.*', '$1') })
    $built = @(Get-ChildItem $libDir -Recurse -Filter 'miniatura.png').Count
    Check "el panel muestra las últimas hojas del elegido ($built miniaturas armadas) y al volver salen de la memoria ($(($minis | Select-Object -Last 2) -join ' y ') ms)" `
        ($built -ge 3 -and $minis.Count -ge 3 -and ($minis | Select-Object -Last 2 | Where-Object { $_ -ge 50 }).Count -eq 0) ($minis -join ' | ')
    BoKey 0x0D # Enter
    $l = @(Get-Content $bocLog -Encoding utf8 | Where-Object { $_ -match 'Bocetos: (elegido|abrir)' })
    Check 'las flechas eligen otro cuaderno y Enter lo abre' ($l.Count -ge 5 -and ($l[-1] -match 'abrir')) ($l -join ' | ')
    # HU-137: abre en la última terminada; RePág y AvPág muestran la vecina ya cargada; AvPág llega al
    # aviso de la siguiente; Inicio, Fin, RePág (a la última terminada), + + − y Esc.
    Start-Sleep 3 # la actual y las vecinas se leen en segundo plano
    $mark = @(Get-Content $bocLog -Encoding utf8).Count
    BoKey 0x21; BoKey 0x22; BoKey 0x22 # RePág AvPág AvPág
    BoKey 0x24; BoKey 0x23; BoKey 0x21 # Inicio Fin RePág
    BoKey 0xBB; BoKey 0xBB; Start-Sleep 2; BoKey 0xBD # + + (espera la entera) −
    BoKey 0x54; Start-Sleep 3 # T: al TV de mentira
    BoKey 0x22; Start-Sleep 2 # AvPág: la siguiente (la de aviso, el papel)
    BoKey 0x54                # T: corta
    BoKey 0x1B # Esc
    $all = @(Get-Content $bocLog -Encoding utf8)
    $opened = ($all | Where-Object { $_ -match 'Cuaderno: (abierto|hoja \d+ en)' } | Select-Object -First 2) -join ' | '
    Check "abre el cuaderno en la última hoja terminada, ocupando la ventana ($opened)" ($opened -match 'abierto .*\| .*Cuaderno: hoja [234] en') $opened
    # HU-144: la hoja queda a la izquierda de la barra vertical.
    $bar = ($all | Where-Object { $_ -match 'Cuaderno: abierto' } | Select-Object -First 1)
    $barOk = $bar -match 'hoja en ([\d.]+),[\d.]+ ([\d.]+) x [\d.]+, barra desde x (\d+)' -and ([double]$Matches[1] + [double]$Matches[2]) -le [double]$Matches[3]
    Check 'la hoja se ve a la izquierda de la barra del cuaderno' $barOk $bar
    $l = @($all | Select-Object -Skip $mark)
    $pages = @($l | Where-Object { $_ -match 'Cuaderno: hoja \d+ (en|·)' })
    $flips = @($pages | Select-Object -First 2 | ForEach-Object { if ($_ -match 'en ([\d.]+) ms \(precargada\)') { [double]$Matches[1] } else { 999 } })
    $flipsOk = $flips.Count -eq 2 -and ($flips | Where-Object { $_ -ge 100 }).Count -eq 0
    Check "RePág y AvPág pasan de hoja en menos de 100 ms ($($flips -join ' y ') ms) y AvPág llega a la siguiente en blanco" `
        ($flipsOk -and $pages.Count -ge 3 -and $pages[2] -match 'en blanco') ($pages -join ' | ')
    $l = $l -join ' '
    $tv = @(if (Test-Path $tvLog) { Get-Content $tvLog -Encoding utf8 })
    $sets = @($tv | Where-Object { $_ -match '^SetAVTransportURI http://127\.0\.0\.1:8766/hoja/' })
    $gets = @($tv | Where-Object { $_ -match '^GET http://127\.0\.0\.1:8766/hoja/\S+ 200 \d+ image/jpeg' })
    $tvOk = $l -match 'TV: conectado a «TV de prueba»' -and $sets.Count -ge 2 -and $gets.Count -ge 2 -and ($tv -contains 'Play') -and ($tv -contains 'Stop') -and $l -match 'TV: cortado'
    Check "T muestra la hoja en el TV (de mentira), AvPág manda la siguiente y T corta ($($sets.Count) hojas, $($gets.Count) descargadas)" $tvOk ($tv -join ' | ')
    Check 'Inicio, Fin, + + − sobre una terminada (se lee entera) y Esc vuelve a la biblioteca' `
        ($l -match 'Cuaderno: hoja 1 en' -and $l -match 'zoom 150 %.*zoom 225 %.*zoom 150 %' -and $l -match 'entera, leída' -and $l -match 'vuelve a la biblioteca') $l
    # HU-138: D abre la siguiente hoja en Cartuchera (--hoja) y Bocetos espera; Ctrl+Q la cierra sin
    # tocar y Bocetos sigue en la biblioteca. En el cuaderno abierto, D otra vez, Ctrl+Enter y Ctrl+Enter
    # (en blanco pregunta) la terminan y Bocetos la muestra. F11 entra y sale de pantalla completa.
    function BoCartuchera {
        foreach ($i in 1..40) { # arranca y toma la pantalla
            $c = Get-Process cartuchera -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($c -and $c.MainWindowHandle -ne 0) { break }
            Start-Sleep -Milliseconds 250
        }
        Start-Sleep 3
        if ($c) { $null = (New-Object -ComObject WScript.Shell).AppActivate($c.Id) }
        Start-Sleep 1
        return $c
    }
    function BoBack($c) {
        $done = $c -and $c.WaitForExit(15000)
        Start-Sleep 2
        $null = (New-Object -ComObject WScript.Shell).AppActivate($app.Id)
        Start-Sleep 1
        return $done
    }
    $mark = @(Get-Content $bocLog -Encoding utf8).Count
    BoKey 0x44 # D
    $c = BoCartuchera
    BoCtrl 0x51 # Ctrl+Q en Cartuchera
    $back = BoBack $c
    $l = @(Get-Content $bocLog -Encoding utf8 | Select-Object -Skip $mark) -join ' '
    Check 'D abre la siguiente hoja en Cartuchera y, cerrada sin tocar, Bocetos sigue en la biblioteca' `
        ($back -and $l -match 'dibujar «Cuaderno \d+» · hoja (\d+) en Cartuchera.*volvió de Cartuchera: hoja \d+ sin tocar' -and $l -notmatch 'Cuaderno: abierto') $l
    $mark = @(Get-Content $bocLog -Encoding utf8).Count
    BoKey 0x0D # Enter: el cuaderno
    BoKey 0x44 # D
    $c = BoCartuchera
    BoCtrl 0x0D; BoCtrl 0x0D # Ctrl+Enter en blanco pregunta; Ctrl+Enter la termina
    $back = BoBack $c
    Start-Sleep 1
    $l = @(Get-Content $bocLog -Encoding utf8 | Select-Object -Skip $mark) -join ' '
    $sheetNo = if ($l -match 'hoja (\d+) en Cartuchera') { $Matches[1] } else { '?' }
    Check "desde el cuaderno, D, Ctrl+Enter y Ctrl+Enter terminan la hoja $sheetNo y Bocetos la muestra" `
        ($back -and $l -match "volvió de Cartuchera: hoja $sheetNo terminada" -and $l -match "Cuaderno: hoja $sheetNo en") $l
    BoKey 0x1B # Esc
    BoKey 0x7A; BoKey 0x7A # F11 F11
    $l = @(Get-Content $bocLog -Encoding utf8 | Select-Object -Skip $mark) -join ' '
    Check 'F11 entra y sale de pantalla completa' ($l -match 'Bocetos: pantalla completa.*Bocetos: ventana común') $l
    # HU-136: Ctrl+N, un nombre y Enter crean el cuaderno; F2, otro nombre y Enter lo renombran; Supr y
    # Supr lo mandan a la papelera.
    function BoType([string]$text) {
        foreach ($ch in $text.ToCharArray()) {
            $vk = [byte][char]::ToUpper($ch)
            [Smoke.Keys]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 20
            [Smoke.Keys]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        }
        Start-Sleep -Milliseconds 300
    }
    BoCtrl 0x4E # Ctrl+N
    BoType 'PRUEBA'
    BoKey 0x0D
    foreach ($i in 1..20) { # crearlo puede tardar un momento
        $created = Test-Path (Join-Path $libDir 'prueba\cuaderno.json')
        if ($created) { break }
        Start-Sleep -Milliseconds 250
    }
    Start-Sleep -Milliseconds 300
    $l = @(Get-Content $bocLog -Encoding utf8) -join ' '
    Check 'Ctrl+N crea un cuaderno' ($created -and $l -match 'cuaderno nuevo «prueba» en prueba') $l
    BoKey 0x71 # F2: el nuevo queda elegido
    BoKey 0x23 # Fin: el nombre viene seleccionado
    BoType 'X'
    BoKey 0x0D
    $name = if ($created) { (Get-Content (Join-Path $libDir 'prueba\cuaderno.json') -Raw | ConvertFrom-Json).name } else { '' }
    Check 'F2 lo renombra' ($name -eq 'pruebax') "nombre: $name"
    # Supr y Supr, como tecla extendida (sin esa marca llega como el Supr del teclado numérico, con
    # el modificador del teclado numérico, y la confirmación lo toma como otra tecla: cancela).
    foreach ($n in 1..2) {
        [Smoke.Keys]::keybd_event(0x2E, 0, 1, [UIntPtr]::Zero); Start-Sleep -Milliseconds 30
        [Smoke.Keys]::keybd_event(0x2E, 0, 3, [UIntPtr]::Zero); Start-Sleep -Milliseconds 500
    }
    foreach ($i in 1..20) { # mandar a la papelera tarda un momento
        $l = @(Get-Content $bocLog -Encoding utf8) -join ' '
        if ($l -match 'a la papelera') { break }
        Start-Sleep -Milliseconds 250
    }
    Check 'Supr y Supr lo mandan a la papelera' (-not (Test-Path (Join-Path $libDir 'prueba')) -and $l -match '«pruebax» a la papelera') $l

    BoCtrl 0x51 # Ctrl+Q
    Check 'Ctrl+Q cierra Bocetos' $app.WaitForExit(10000) ''
    $app = $null

    # HU-142: el cuaderno de Krita, en su propia biblioteca.
    $kDir = Join-Path $libDir 'krita\personajes'
    New-Item -ItemType Directory -Force $kDir | Out-Null
    @{ version = 2; app = 'krita'; name = 'Personajes en Krita'; collection = ''; cover = '#714765'; maxSheets = 10; started = '2026-10-13'
       updated = '2026-10-13'; paper = @{ preset = 'tonadoTostado'; color = '#d6c4a1'; grain = 1600; texture = 20 } } |
        ConvertTo-Json -Depth 4 | Set-Content (Join-Path $kDir 'cuaderno.json') -Encoding utf8
    $app = Start-Process $boc -ArgumentList '--biblioteca', "`"$(Join-Path $libDir 'krita')`"", '--krita', "`"$(Join-Path $root 'build\release\fake_krita.exe')`"", '--exportar-en', "`"$(Join-Path $libDir 'export')`"" -PassThru
    Start-Sleep 3
    $null = (New-Object -ComObject WScript.Shell).AppActivate($app.Id)
    Start-Sleep 1
    BoKey 0x44 # D: el papel (Cartuchera --papel), la hoja y Krita, que guarda y sale
    foreach ($i in 1..60) {
        $l = @(Get-Content $bocLog -Encoding utf8) -join ' ' # el log empieza de nuevo con cada Bocetos
        if ($l -match 'volvió de Krita') { break }
        Start-Sleep -Milliseconds 250
    }
    Start-Sleep 2
    $null = (New-Object -ComObject WScript.Shell).AppActivate($app.Id)
    Start-Sleep 1
    $l = @(Get-Content $bocLog -Encoding utf8) -join ' ' # el log empieza de nuevo con cada Bocetos
    $sheet1 = Join-Path $kDir 'hoja-0001'
    Check 'D en un cuaderno de Krita arma el papel, abre la hoja en Krita y, guardada, Bocetos muestra el borrador' `
        ((Test-Path (Join-Path $kDir 'papel.png')) -and (Test-Path (Join-Path $sheet1 'borrador.png')) -and $l -match 'Bocetos: Krita en .*fake_krita' -and
         $l -match 'papel listo.*dibujar «Personajes en Krita» · hoja 1 en Krita.*volvió de Krita: hoja 1 en borrador' -and $l -match 'Cuaderno: hoja 1 \(borrador de Krita\) en') $l
    BoCtrl 0x0D; BoCtrl 0x0D # Ctrl+Enter pregunta; Ctrl+Enter la termina
    foreach ($i in 1..20) {
        if (Test-Path (Join-Path $sheet1 'terminada.png')) { break }
        Start-Sleep -Milliseconds 250
    }
    Start-Sleep 1
    $l = @(Get-Content $bocLog -Encoding utf8) -join ' ' # el log empieza de nuevo con cada Bocetos
    Check 'Ctrl+Enter y Ctrl+Enter terminan el borrador de Krita y Bocetos muestra la hoja terminada' `
        ((Test-Path (Join-Path $sheet1 'terminada.png')) -and $l -match 'Krita: confirmar terminar la hoja 1 .*Krita: hoja 1 terminada' -and $l -match 'Cuaderno: hoja 1 en') $l
    $exportDir = Join-Path $libDir 'export'
    BoCtrl 0x45 # Ctrl+E: la hoja 1, que se está mirando
    BoCtrl 0x50 # Ctrl+P: el PDF
    $pdf = Join-Path $exportDir 'Personajes en Krita.pdf'
    foreach ($i in 1..40) {
        if (@(Get-Content $bocLog -Encoding utf8) -match 'Exportar: PDF listo') { break }
        Start-Sleep -Milliseconds 250
    }
    $png = Join-Path $exportDir 'Personajes en Krita - hoja 01.png'
    $same = (Test-Path $png) -and ((Get-FileHash $png).Hash -eq (Get-FileHash (Join-Path $sheet1 'terminada.png')).Hash)
    $pdfPages = if (Test-Path $pdf) { ([regex]::Matches([IO.File]::ReadAllText($pdf, [Text.Encoding]::GetEncoding(28591)), '/Type\s*/Page[^s]')).Count } else { 0 }
    $l = @(Get-Content $bocLog -Encoding utf8) -join ' '
    Check "Ctrl+E guarda la hoja como PNG (idéntica) y Ctrl+P el cuaderno como PDF ($pdfPages páginas)" ($same -and $pdfPages -eq 2 -and $l -match 'Exportar: hoja 1 en' -and $l -match 'Exportar: PDF listo, 2 páginas') $l
    Start-Sleep 1
    BoCtrl 0x51 # Ctrl+Q
    if (-not $app.WaitForExit(10000)) { $app.Kill() }
    $app = $null
    Get-ChildItem $libDir -Recurse -File | ForEach-Object { $_.IsReadOnly = $false } # el .kra terminado queda de solo lectura
}
finally {
    Get-Process cartuchera -ErrorAction SilentlyContinue | Stop-Process -Force # la que lanzó D, si quedó (Bocetos no se cierra mientras se dibuja)
    if ($tvProc -and -not $tvProc.HasExited) { $tvProc.Kill() }
    Remove-Item $tvLog -Force -ErrorAction SilentlyContinue
    if ($app -and -not $app.HasExited) {
        $null = $app.CloseMainWindow()
        if (-not $app.WaitForExit(15000)) { $app.Kill() }
    }
    Remove-Item $libDir -Recurse -Force -ErrorAction SilentlyContinue
    if ($bocBackup) { [IO.File]::WriteAllBytes($config, $bocBackup) }
}

if ($failures.Count) { Write-Host "$($failures.Count) falla(s)"; exit 1 }
Write-Host 'Prueba de la app real: todo bien'
