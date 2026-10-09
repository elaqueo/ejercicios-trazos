# Spike HU-43: resume la entrada del lápiz medida por el spike (WM_POINTER) y por Qt
# (log de paintcore con QT_LOGGING_RULES="paintcore.input.debug=true").
#   pwsh analizar.ps1                        # último CSV del spike y el log de ejercicios
#   pwsh analizar.ps1 -Csv x.csv -QtLog y.log
param(
    [string]$Csv,
    [string]$QtLog = "$env:LOCALAPPDATA\trazos\ejercicios.log"
)
$ErrorActionPreference = 'Stop'

function Percentil([double[]]$values, [double]$p) {
    if ($values.Count -eq 0) { return [double]::NaN }
    $sorted = $values | Sort-Object
    return $sorted[[Math]::Min($sorted.Count - 1, [int][Math]::Floor($p * $sorted.Count))]
}

# Intervalos (ms) entre muestras consecutivas de un mismo trazo, en baldes.
function Baldes([double[]]$deltas) {
    $limits = @(0, 1, 3, 4.5, 5.5, 7, 10, 16, 20)
    $names = @('0', '0-1', '1-3', '3-4,5', '4,5-5,5', '5,5-7', '7-10', '10-16', '16-20', '>20')
    $counts = @(0) * $names.Count
    foreach ($d in $deltas) {
        if ($d -eq 0) { $counts[0]++; continue }
        $i = 1
        while ($i -lt $limits.Count -and $d -gt $limits[$i]) { $i++ }
        $counts[$i]++
    }
    $total = [Math]::Max(1, $deltas.Count)
    return ($names | ForEach-Object -Begin { $k = 0 } -Process { '{0}: {1} ({2:P0})' -f $_, $counts[$k], ($counts[$k] / $total); $k++ }) -join '  '
}

function Resumen([string]$titulo, $trazos, [string]$reloj) {
    # $trazos: lista de listas de timestamps en ms.
    $deltas = [System.Collections.Generic.List[double]]::new()
    $muestras = 0; $duracion = 0.0
    foreach ($t in $trazos) {
        if ($t.Count -lt 2) { continue }
        $muestras += $t.Count
        $duracion += ($t[-1] - $t[0])
        for ($i = 1; $i -lt $t.Count; $i++) { $deltas.Add($t[$i] - $t[$i - 1]) }
    }
    $positivos = @($deltas | Where-Object { $_ -gt 0 })
    "== $titulo ($reloj)"
    '  trazos: {0}   muestras en contacto: {1}   duración: {2:N2} s' -f $trazos.Count, $muestras, ($duracion / 1000)
    if ($duracion -gt 0) { '  muestras/s (promedio dentro de los trazos): {0:N1}' -f (($muestras - $trazos.Count) / ($duracion / 1000)) }
    '  intervalo repetido (0 ms): {0:P1}' -f (@($deltas | Where-Object { $_ -eq 0 }).Count / [Math]::Max(1, $deltas.Count))
    '  intervalo > 0: mediana {0:N2} ms   p95 {1:N2} ms   máximo {2:N2} ms   mínimo {3:N3} ms' -f (Percentil $positivos 0.5), (Percentil $positivos 0.95), (($positivos | Measure-Object -Maximum).Maximum), (($positivos | Measure-Object -Minimum).Minimum)
    '  baldes: ' + (Baldes $deltas)
}

# --- Spike (WM_POINTER) -----------------------------------------------------------
if (-not $Csv) {
    $Csv = Get-ChildItem "$env:LOCALAPPDATA\trazos\spikes\entrada-*.csv" | Sort-Object LastWriteTime | Select-Object -Last 1 -ExpandProperty FullName
}
if ($Csv) {
    $header = Get-Content $Csv -TotalCount 5 | Where-Object { $_.StartsWith('#') }
    $hz = [double](($header | Select-String 'qpc_hz=(\d+)').Matches[0].Groups[1].Value)
    $rows = Get-Content $Csv | Where-Object { -not $_.StartsWith('#') } | ConvertFrom-Csv
    "Spike: $Csv"
    $header | ForEach-Object { "  $_" }

    $porPerf = [System.Collections.Generic.List[object]]::new()
    $porTime = [System.Collections.Generic.List[object]]::new()
    $actualP = $null; $actualT = $null
    foreach ($r in $rows) {
        if ($r.in_contact -eq '1') {
            if ($null -eq $actualP) { $actualP = [System.Collections.Generic.List[double]]::new(); $actualT = [System.Collections.Generic.List[double]]::new() }
            $actualP.Add([double]$r.perf_count * 1000 / $hz)
            $actualT.Add([double]$r.time_ms)
        } elseif ($null -ne $actualP) {
            $porPerf.Add($actualP); $porTime.Add($actualT); $actualP = $null; $actualT = $null
        }
    }
    if ($null -ne $actualP) { $porPerf.Add($actualP); $porTime.Add($actualT) }

    Resumen 'WM_POINTER' $porPerf 'PerformanceCount'
    Resumen 'WM_POINTER' $porTime 'dwTime (ms)'

    $mensajes = $rows | Where-Object { $_.in_contact -eq '1' } | Group-Object msg | ForEach-Object { [int]$_.Group[0].msg_samples }
    '  muestras por mensaje: ' + (($mensajes | Group-Object | Sort-Object { [int]$_.Name } | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join '  ')

    # Latencia de lectura: desde el PerformanceCount de la muestra más nueva hasta que el mensaje llegó a la ventana.
    $lat = $rows | Where-Object { $_.in_contact -eq '1' -and $_.hist_index -eq '0' } | ForEach-Object { ([double]$_.recv_qpc - [double]$_.perf_count) * 1000 / $hz }
    '  muestra → ventana: mediana {0:N2} ms   p95 {1:N2} ms' -f (Percentil $lat 0.5), (Percentil $lat 0.95)

    $c = $rows | Where-Object { $_.in_contact -eq '1' }
    $pres = $c | ForEach-Object { [int]$_.pressure } | Measure-Object -Minimum -Maximum
    $tx = $c | ForEach-Object { [int]$_.tilt_x } | Measure-Object -Minimum -Maximum
    $ty = $c | ForEach-Object { [int]$_.tilt_y } | Measure-Object -Minimum -Maximum
    $rot = @($c | Where-Object { [int]$_.rotation -ne 0 }).Count
    $goma = @($rows | Where-Object { ([Convert]::ToInt32($_.pen_flags, 16) -band 0x6) -ne 0 }).Count
    $lateral = @($rows | Where-Object { ([Convert]::ToInt32($_.pen_flags, 16) -band 0x1) -ne 0 }).Count
    '  presión {0}..{1} (de 1024)   tilt X {2}..{3}   tilt Y {4}..{5}   rotación ≠ 0: {6}   goma: {7}   botón lateral: {8}' -f $pres.Minimum, $pres.Maximum, $tx.Minimum, $tx.Maximum, $ty.Minimum, $ty.Maximum, $rot, $goma, $lateral
    ''
}

# --- Qt (paintcore) ---------------------------------------------------------------
if (Test-Path $QtLog) {
    # Línea: "HH:mm:ss.zzz debug paintcore.input: fase=1 trazo=true pos=... ts=46005031 dt_ms=0".
    # Dos relojes: timestamp() del evento y la hora en que paintcore lo procesó.
    $trazos = [System.Collections.Generic.List[object]]::new()
    $recibido = [System.Collections.Generic.List[object]]::new()
    $actual = $null; $actualR = $null
    foreach ($line in Get-Content $QtLog) {
        if ($line -notmatch '^(\d\d):(\d\d):(\d\d)\.(\d{3}) .*fase=(\d) trazo=(\w+) .* ts=(\d+)') { continue }
        $wall = ((([double]$Matches[1] * 60) + [double]$Matches[2]) * 60 + [double]$Matches[3]) * 1000 + [double]$Matches[4]
        $fase = [int]$Matches[5]; $trazo = $Matches[6] -eq 'true'; $ts = [double]$Matches[7]
        if ($fase -eq 0) {
            $actual = [System.Collections.Generic.List[double]]::new(); $trazos.Add($actual)
            $actualR = [System.Collections.Generic.List[double]]::new(); $recibido.Add($actualR)
        }
        if ($null -ne $actual -and ($fase -eq 0 -or $trazo)) { $actual.Add($ts); $actualR.Add($wall) }
        if ($fase -eq 2) { $actual = $null; $actualR = $null }
    }
    "Qt: $QtLog"
    Resumen 'QTabletEvent (paintcore)' $trazos 'timestamp() en ms'
    Resumen 'QTabletEvent (paintcore)' $recibido 'hora de proceso en ms'
}
