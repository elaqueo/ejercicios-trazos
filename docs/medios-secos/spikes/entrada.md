# Spike HU-43 · Entrada del lápiz por WM_POINTER

Medido el 8 y 9 de octubre de 2026. Código en [`spikes/cartuchera-entrada`](../../../spikes/cartuchera-entrada): `main.cpp` (Win32 puro, registra cada muestra en un CSV) y `analizar.ps1` (resume el CSV del spike y el log de `paintcore` con `QT_LOGGING_RULES="paintcore.input.debug=true"`).

## Resultados

| | Desktop (i5 Skylake) | Laptop (i7 11.ª gen) |
| --- | --- | --- |
| Muestras/s dentro de un trazo (WM_POINTER) | 132,9 | pendiente |
| Muestras/s (QTabletEvent, paintcore) | 133,9 | pendiente |
| Intervalo entre muestras | 88 % ≈ 8 ms, 12 % ≈ 4 ms; máximo 16 ms | pendiente |
| Muestras por mensaje `WM_POINTER` | 1 (99,8 %); 2 (0,2 %) | pendiente |
| Timestamp `PerformanceCount` | 0,1 µs de resolución, nunca repetido | pendiente |
| Timestamp `QTabletEvent::timestamp()` | saltos de 16 ms; 52 % repetidos | pendiente |
| Muestra → ventana (`PerformanceCount` → recepción) | mediana 0,39 ms, p95 0,50 ms | pendiente |
| Presión | 0..965 de 1024 | pendiente |
| Inclinación | X 4..56°, Y −29..54° | pendiente |
| Goma y botón lateral | llegan (`penFlags`) | pendiente |
| Rotación | no llega (sin Art Pen) | pendiente |

Driver de Wacom: versión a confirmar en cada máquina (el plan fija la 6.3.41).

## Hallazgos

1. **No llegan 200 muestras/s sino ~133.** El patrón (casi todo a 8 ms, algunos pares a 4 ms) sugiere que el driver entrega a 125 Hz con alguna muestra intercalada. Qt ve exactamente las mismas muestras: la pérdida no es de Qt ni de `WM_POINTER`. A 133 Hz hay unas 2,2 muestras por frame de 60 Hz; la regla de integrar todas sigue valiendo y el barrido continuo entre muestras (no dabs) se vuelve más importante.
2. **Casi nunca hay historial**: una muestra por mensaje. `GetPointerPenInfoHistory` igual hay que usarlo (cuando el hilo se atrasa, junta), pero no es la fuente principal de muestras.
3. **El timestamp de Qt no sirve para Cartuchera**: resolución de 16 ms y la mitad repetidos. `PerformanceCount` es exacto y es el que permite medir latencia y derivar velocidad sin filtro raro. El reloj de trazo de `paintcore` existe por esto (HU-46 lo elimina).
4. **La latencia de lectura es despreciable**: la muestra llega a la ventana medio milisegundo después de su `PerformanceCount`.
5. **Posición: usar `ptHimetricLocation`, no `ptPixelLocation` ni `ptHimetricLocationRaw`.**
    1. `ptPixelLocation` está cuantizado al píxel (≈ 0,19 mm con el mapeo actual).
    2. `ptHimetricLocationRaw` no son coordenadas físicas: Windows lo recalcula desde el píxel con el rectángulo que informa el driver (error ≤ 1 px), así que también está cuantizado.
    3. `ptHimetricLocation` avanza de a 1 unidad, ≈ 7,3 unidades por píxel: ≈ 26 µm por paso, mejor que la precisión de la tableta (±0,25 mm). Es lo que usa Qt para sus posiciones con decimales.
6. **El rectángulo que informa el driver no es el mapeo real.** `GetPointerDeviceRects` dice tableta 32513 × 20321 (0,01 mm) → pantalla 4480 × 1080 (los dos monitores), pero la tableta está mapeada a una porción de 1734 × 1081 del ultrawide. Para pasar a milímetros hace falta la calibración de dos esquinas de `appkit` (área útil = área activa completa de la tableta), no `GetPointerDeviceRects`.

## Decisión para `libs/tabletinput`

1. Fuente: `WM_POINTER*` de un HWND propio, con `GetPointerPenInfoHistory` en cada mensaje y las muestras en orden (el historial viene de la más nueva a la más vieja).
2. Por muestra: `ptHimetricLocation` (posición), `PerformanceCount` (tiempo), `pressure` (0..1024), `tiltX`/`tiltY`, `rotation` solo si `penMask` la marca, `penFlags` (goma por `PEN_FLAG_INVERTED`/`PEN_FLAG_ERASER`, botón lateral por `PEN_FLAG_BARREL`), y `POINTER_FLAG_INCONTACT` para separar contacto de proximidad.
3. Conversión a milímetros: himétrico → píxel (con el rectángulo de `GetPointerDeviceRects`) → milímetros con el área calibrada en `appkit`, que corresponde al área activa completa (325,1 × 203,2 mm).
4. Responder `WM_POINTER*` del lápiz sin pasar a `DefWindowProc`, para que Windows no sintetice mouse.
5. Diseñar para ~133 Hz reales, no 200.

## Pendiente para cerrar HU-43

1. Correr lo mismo en la laptop (paquete portable: `cartuchera-entrada.exe` + `analizar.ps1`).
2. Anotar la versión del driver de Wacom en las dos máquinas.
3. Si en alguna da 200 Hz, ver qué cambia (driver, USB, modo de la tableta).
