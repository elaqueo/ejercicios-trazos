# Spike HU-44: latencia por modo de presentación, a partir del CSV del spike.
#   pwsh analizar.ps1               # último swapchain-*.csv
#   pwsh analizar.ps1 -Csv x.csv
param([string]$Csv)
$ErrorActionPreference = 'Stop'
if (-not $Csv) {
    $Csv = Get-ChildItem "$env:LOCALAPPDATA\trazos\spikes\swapchain-*.csv" | Sort-Object LastWriteTime | Select-Object -Last 1 -ExpandProperty FullName
}
$header = Get-Content $Csv -TotalCount 1
$hz = [double](($header | Select-String 'qpc_hz=(\d+)').Matches[0].Groups[1].Value)
$rows = Get-Content $Csv | Where-Object { -not $_.StartsWith('#') } | ConvertFrom-Csv
"CSV: $Csv"
"  $header"

function P([double[]]$v, [double]$p) {
    if ($v.Count -eq 0) { return [double]::NaN }
    $s = $v | Sort-Object
    $s[[Math]::Min($s.Count - 1, [int][Math]::Floor($p * $s.Count))]
}

# Grupos: modo, y en el modo 2 también el adelanto.
$groups = $rows | Group-Object { if ($_.mode -eq '2') { "2 · adelanto $($_.lead_ms) ms" } else { $_.mode } } | Sort-Object Name
$names = @{ '0' = '0 · ingenuo'; '1' = '1 · flip + waitable'; '3' = '3 · tearing' }
foreach ($g in $groups) {
    $frames = $g.Group
    $con = @($frames | Where-Object { [int]$_.samples -gt 0 })
    $vsync = [double[]]@($con | Where-Object { $_.sync_qpc -ne '0' } | ForEach-Object { ([double]$_.sync_qpc - [double]$_.newest_sample_qpc) * 1000 / $hz })
    $present = [double[]]@($con | ForEach-Object { ([double]$_.present_qpc - [double]$_.newest_sample_qpc) * 1000 / $hz })
    $edad = [double[]]@($con | ForEach-Object { ([double]$_.drain_qpc - [double]$_.newest_sample_qpc) * 1000 / $hz })
    # Frames salteados: vsyncs consecutivos separados por más de 1,5 períodos (≈ 25 ms a 60 Hz).
    $syncs = @($frames | Where-Object { $_.sync_qpc -ne '0' } | ForEach-Object { [double]$_.sync_qpc * 1000 / $hz })
    $saltos = 0
    for ($i = 1; $i -lt $syncs.Count; $i++) { if (($syncs[$i] - $syncs[$i - 1]) -gt 25) { $saltos++ } }
    $nombre = if ($names.ContainsKey($g.Name)) { $names[$g.Name] } else { $g.Name }
    "== $nombre   (frames $($frames.Count), con muestras $($con.Count))"
    if ($vsync.Count) { '  muestra → vsync: mediana {0:N1} ms   p95 {1:N1} ms   mínimo {2:N1} ms' -f (P $vsync 0.5), (P $vsync 0.95), (P $vsync 0) }
    '  muestra → Present: mediana {0:N1} ms   p95 {1:N1} ms' -f (P $present 0.5), (P $present 0.95)
    '  edad de la muestra al tomarla: mediana {0:N1} ms' -f (P $edad 0.5)
    if ($syncs.Count) { '  vsyncs salteados: {0} de {1}' -f $saltos, $syncs.Count }
}
