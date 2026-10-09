# Spike HU-45 · Kernel de contacto en CPU (AVX2 y AVX-512)

Medido el 9 de octubre de 2026. Código en [`spikes/cartuchera-kernel`](../../../spikes/cartuchera-kernel): `kernel_scalar.cpp`, `kernel_avx2.cpp` (`/arch:AVX2`), `kernel_avx512.cpp` (`/arch:AVX512`) y `main.cpp`, que elige la ruta según la CPU, mide y compara hashes. Compilado con el preset `release`.

## Qué simula

Versión de juguete del núcleo del plan, toda en enteros de 16 bits con saturación:

1. Papel en tiles de 64 × 64 a 600 dpi, creados al tocarlos, con relieve procedural (semilla fija) y depósito.
2. Punta como mapa de alturas (cono elíptico).
3. Por subpaso (≤ 1 celda): búsqueda binaria de 16 pasos de la profundidad a la que la fuerza total iguala la presión, y depósito ∝ penetración × distancia × (1 − saturación).
4. Trazos sintéticos con semilla fija a 133 muestras/s (lo medido en HU-43), 1500 muestras por caso.

## Resultados (ms por muestra)

| Caso | Desktop i5-6400 · AVX2 | Laptop i7-1195G7 · AVX2 | Laptop · AVX-512 | Escalar (desktop / laptop) |
| --- | --- | --- | --- | --- |
| Punta HB, 100 mm/s (576 celdas) | 0,05 · p99 0,14 | 0,02 · p99 0,06 | 0,03 · p99 0,07 | 0,25 / 0,17 |
| Punta HB, garabato 1000 mm/s (162 subpasos) | 0,47 · p99 0,95 | 0,26 · p99 0,42 | 0,28 · p99 0,47 | 2,41 / 1,45 |
| Lápiz de costado, 300 mm/s (4096 celdas) | 0,60 · p99 1,01 | 0,34 · p99 0,46 | 0,36 · p99 0,44 | 4,79 / 2,99 |
| Pastel de costado, 300 mm/s (4096 celdas) | 0,52 · p99 0,88 | 0,28 · p99 0,36 | 0,31 · p99 0,57 | 4,71 / 2,91 |

Valores: mediana · p99. Los máximos aislados llegan a 10 ms (desktop) y 6 ms (laptop) en el garabato rápido.

**Determinismo:** el hash del papel es **idéntico** en las tres rutas y en **las dos máquinas**, en los cuatro casos (`74c09599ae2781db`, `930c8b16497e26ed`, `2626be79fd9b9058`, `b5028a60bf7b5b2f`).

## Hallazgos

1. **La simulación en CPU entra con margen.** En el peor caso de la desktop, AVX2 da 0,6 ms de mediana y ~1 ms de p99 por muestra; a ~2,2 muestras por frame son ~1,3 ms de los 3 ms que el plan reserva. La laptop va casi al doble de rápido.
2. **AVX2 es imprescindible:** el escalar es 5 a 10 veces más lento y no entra en los casos grandes (≈ 4,8 ms por muestra).
3. **AVX-512 no suma nada:** en la laptop da lo mismo o un poco peor que AVX2. Con huellas de 576 a 4096 celdas el trabajo es poco por llamada, y el tiempo se va también en copiar la huella del papel y buscar tiles, que no se aceleran con vectores más anchos.
4. **El determinismo entre máquinas funciona** con aritmética entera y saturación definidas igual en todas las rutas: es la base del replay y de las pruebas de regresión por hash del plan.
5. **Los picos (hasta 10 ms) son de creación de tiles**, no del kernel: aparecen en los casos que tocan más tiles nuevos (garabato: 3204 tiles), y cada tile nuevo genera su relieve y reserva memoria en mitad del trazo. Hipótesis a confirmar en la Fase 1 midiendo por separado.

## Decisión para `drymedia`

1. **Simulación en CPU**, como propone el plan. No se pasa a GPU.
2. **Una sola ruta vectorizada: AVX2**, más la escalar como referencia y para pruebas. AVX-512 queda descartado (no mejora y suma una ruta a mantener bit a bit).
3. Aritmética entera de 16 bits con saturación, con la misma definición en todas las rutas; prueba de regresión por hash del papel desde el primer commit de `drymedia`.
4. Tiles: reservarlos y generar su relieve **fuera del camino crítico** (pool y generación anticipada alrededor de la punta) para eliminar los picos.
5. Copiar la huella entre tiles y arreglos contiguos cuesta; en la Fase 1 medir qué parte del tiempo es copia y evaluar operar directo sobre los tiles.
