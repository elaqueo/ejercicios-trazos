# Spike HU-44 · Swapchain D3D11 y latencia

Medido el 9 de octubre de 2026 en la desktop (i5 Skylake, GTX 960, LG ultrawide 2560 × 1080 a 60 Hz). Código en [`spikes/cartuchera-swapchain`](../../../spikes/cartuchera-swapchain): `main.cpp` y `analizar.ps1`.

## Montaje

1. Cascarón Qt: un `QWidget` sin bordes que cubre el monitor. Adentro, un **HWND hijo Win32 puro** (no un widget de Qt) recibe `WM_POINTER` como en HU-43 (`ptHimetricLocation`, `PerformanceCount`) y encola las muestras. Qt y D3D11 conviven sin problemas.
2. Hilo de render propio: toma las muestras, dibuja los segmentos sobre una textura persistente, la copia al backbuffer y presenta en un swapchain `FLIP_DISCARD`.
3. Medición interna: `PerformanceCount` de la muestra más nueva de cada frame contra el vsync en que ese frame se mostró (`GetFrameStatistics`, exacto, no estimado). **No incluye** el barrido de pantalla ni la respuesta del panel, ni el tramo USB/driver previo al `PerformanceCount`. Sin medición externa (decisión del Product Owner).

## Resultados

| Modo | Muestra → vsync (mediana) | p95 | Vsyncs perdidos | Sensación |
| --- | --- | --- | --- | --- |
| 0 · ingenuo (latencia 3, sin waitable) | 87,2 ms | 91,1 ms | 0 % | la línea persigue a la punta |
| 1 · flip + waitable (latencia 1) | 20,8 ms | 24,7 ms | 0 % | |
| 2 · justo a tiempo, adelanto < 3,5 ms | 21–23 ms | — | casi todos | peor que el modo 1 |
| 2 · justo a tiempo, adelanto 4–5 ms | 8,5–9,8 ms | 23–27 ms | 6–7 % | |
| **2 · justo a tiempo, adelanto 7,5 ms** | **11,6–11,8 ms** | **15,6–16,1 ms** | **≤ 0,1 %** | **por lejos el mejor** |
| 3 · tearing | muestra → `Present` 0,6 ms | 1,5 ms | — | igual que el 2, sin cortes visibles |

1. El modo 0 reproduce los 60–100 ms de las apps genéricas (Krita, Photoshop) que el plan toma como punto de partida.
2. `SetMaximumFrameLatency(1)` con waitable solo ya baja a ~21 ms: es la ganancia grande, como anticipaba el plan.
3. Render justo a tiempo baja otros ~9 ms, pero **el adelanto es crítico**: si el frame llega tarde a la composición se pierde un vsync entero y la latencia sube a la del modo 1 o peor. Con 4–5 ms la mediana es mejor pero el 6–7 % de frames perdidos da tirones; con 7,5 ms no se perdió ninguno en más de 1000 frames.
4. La muestra tiene en promedio ~4 ms de edad cuando se toma (llegan cada ~8 ms, HU-43): es un piso que ningún modo baja.

## Estimación de pen-to-photon

Modo 2 con 7,5 ms de adelanto: 11,7 ms medidos + ~8 ms de barrido (promedio, la hoja está a media altura) + ~5 ms de respuesta del panel + 2–8 ms de USB y driver antes del `PerformanceCount` ≈ **27–33 ms**. Meta del plan: < 40 ms. **Cumplida con margen.** Con el modo 1 serían ~37–42 ms.

## Decisión para Cartuchera

1. Lienzo: HWND hijo Win32 dentro del cascarón Qt, con `WM_POINTER` en ese HWND y render en hilo propio.
2. Swapchain `FLIP_DISCARD`, `SetMaximumFrameLatency(1)`, waitable, y **render justo a tiempo con adelanto adaptativo**: arrancar en ~7,5 ms y ajustarlo con el tiempo de render medido y los vsyncs perdidos (si se pierde uno, subir; si no se pierde ninguno en N frames, bajar de a poco), con un piso que no baje de ~5 ms.
3. Tearing: **opcional y apagado por defecto**. No mostró ventaja percibida ni cortes, pero no se pudo confirmar que fuera tearing real (ver Abierto).
4. Al salir, el hilo principal espera al de render atendiendo mensajes: un `join()` a secas se traba porque `Present` puede necesitar que la ventana responda.
5. D3D11: sin descarte de caras para la tinta (el orden de los vértices depende de la dirección del trazo).

## Abierto

1. **¿El modo 3 hizo tearing real?** Los `Present` salen a 118/s, sin esperar el vsync, pero desde la app no se ve si Windows lo mostró en *independent flip* o lo compuso igual. Se confirma con PresentMon (columna de modo de presentación), sin cambiar código. Mientras tanto, queda apagado.
2. La laptop (GPU integrada) no se midió: el adelanto adaptativo debería absorber la diferencia, y Cartuchera lo va a registrar.
