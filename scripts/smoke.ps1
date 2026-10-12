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
    Check 'I desactiva y vuelve a activar el costado' (($l -join ' ') -match 'Costado: desactivado.*Costado: activado') ($l -join ' | ')

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
    if ($shown -eq 32 -and $flips.Count -ge 30 -and $slow.Count -eq 0) {
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

    # HU-112: A y E no hacen nada (no hay desgaste ni punta seca).
    foreach ($k in 'a', 'e') { $ws.SendKeys($k); Start-Sleep -Milliseconds 400 }
    $extra = @(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Afilada|Punta seca' })
    if ($extra.Count -eq 0) {
        Write-Host '  ok   A y E no hacen nada en Mesa de animación'
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
    $null = $app.CloseMainWindow()
    if (-not $app.WaitForExit(15000)) { $app.Kill() }
    $closed = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Toma: guardada al cerrar en' }) -join ' ')
    $closeMs = if ($closed -match 'en (\d+) ms') { [int]$Matches[1] } else { -1 }
    $app = Start-Process $mesa -ArgumentList '--toma', "`"$takeDir`"" -PassThru
    Start-Sleep 5
    $opened = (@(Get-Content $mesaLog -Encoding utf8 | Where-Object { $_ -match 'Toma abierta de' }) -join ' ')
    if ($closeMs -ge 0 -and $closeMs -lt 2000 -and $opened.Contains($expected)) {
        Write-Host "  ok   al cerrar espera $closeMs ms y la toma abre igual"
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
if ($failures.Count) { Write-Host "$($failures.Count) falla(s)"; exit 1 }
Write-Host 'Prueba de la app real: todo bien'
