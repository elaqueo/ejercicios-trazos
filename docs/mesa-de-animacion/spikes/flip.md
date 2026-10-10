# Spike HU-81 · Flip con hojas reales

10 de octubre de 2026 · desktop (i5 Skylake, GTX 960, 16 GB) · preset release

> Fase 0 de Mesa de animación. Pregunta: ¿cuánto cuesta cambiar de hoja, y qué guarda una hoja que no es la activa? El código está en `spikes/mesa-flip` (descartable; se compila con los otros spikes). Salida completa de la corrida al final.

## Qué se midió

Una hoja real de la app: 7680 × 4800 celdas (toda la tableta a 600 dpi, 120 × 75 tiles de 64 × 64), que en el ultra-wide se ve en 1734 × 1080 px. Los trazos son los grabados con `cartuchera --grabar`; como la grabación es corta (6 trazos, 2 % de los tiles), se repitieron desplazados por la hoja hasta tocar el 10, 25 y 50 % de los tiles, que es el rango de un rough (un rough suelto ronda el 10 %; uno con construcción y repasos, el 25 %).

## Resultados

| Qué | Medición | Lectura |
| --- | --- | --- |
| Relieve y crestas del papel | 143 MB, 250 ms al crearlo | Igual para todas las hojas de una toma: tiene que ser **uno solo, compartido** |
| Estado de simulación de una hoja (4 planos por tile) | 6 MB al 2 % · 32 MB al 11 % · 72 MB al 26 % · 141 MB al 50 % | 120 hojas al 26 % = 8,6 GB: **no entra en RAM** |
| Ese estado comprimido (zlib nivel 1) | 3,6 MB al 11 % · 9,2 MB al 26 % · 27 MB al 50 % (5 a 9 : 1) | 1,1 GB al 26 %: sirve para disco y para un caché acotado, no para tener toda la pila |
| Comprimir / descomprimir una hoja al 26 % | 460 ms / 145 ms | Comprimir solo en el hilo de guardado; descomprimir no cabe en un flip |
| Cargar los tiles de una hoja en el papel de la simulación | 7 ms al 11 % · 14 ms al 26 % · 42 ms al 50 % | Cambiar la hoja **que se simula** no puede ser parte del flip |
| Redibujar la hoja entera desde la simulación (`renderTone`, un hilo) | 108 ms | El flip **no puede redibujar**: cada hoja guarda su imagen |
| Imagen de una hoja | 7,1 MB en BGRA · 1,8 MB en tono de 8 bits | 120 hojas: 857 MB en BGRA, 214 MB en tono |
| Reconstruir BGRA desde el tono de 8 bits | 3,1 ms | Posible, pero el tono de 8 bits no alcanza cuando haya lápices de color (HU-99) |
| Subir la imagen BGRA a la GPU (`UpdateSubresource`, hasta que termina) | 1,6 ms (máx 7 ms) | Entra en un frame, con margen chico en el peor caso |
| 120 texturas BGRA en la GPU | se crearon las 120 (857 MB de VRAM) | La GTX 960 tiene 4 GB |
| Copiar una hoja que ya está en la GPU (`CopyResource`) | 0,16 ms (máx 0,85 ms) | **El flip sale casi gratis** si las hojas viven en la GPU |

## Decisión

El flip y el dibujo son dos cosas distintas, con dos representaciones distintas de la hoja.

1. **Para mostrar, cada hoja es una textura en la GPU.** El flip cambia qué textura presenta el render: una copia de 0,16 ms, o directamente otra textura en el shader de rotación. Cumple RNF-01 con mucho margen y no depende del tamaño de la pila ni de cuánto esté dibujada cada hoja. La mesa de luz (HU-83) es el mismo shader componiendo varias de esas texturas con su tinte.
2. **Para dibujar, solo la hoja activa vive en la simulación.** Las demás guardan su estado comprimido: en disco siempre (es el archivo de la toma, HU-85) y en RAM las vecinas, en un caché con tope (por ejemplo 1 GB). Cuando el flip se detiene (al soltar la tecla), un hilo aparte carga en la simulación el estado de la hoja que quedó arriba. Si el lápiz baja antes de que termine (≈ 150 ms al 26 % sin caché), ese primer trazo espera; con la hoja en el caché, la espera es la carga de tiles (≈ 14 ms).
3. **El relieve es uno por toma.** `drymedia` separa el relieve (de solo lectura, compartido) del conjunto de tiles de depósito, que pasa a ser un objeto por hoja. Así la simulación cambia de hoja cambiando de conjunto de tiles, sin copiar 72 MB ni regenerar el relieve.
4. **La imagen de cada hoja también se guarda en la toma**, para que abrir no tenga que redibujar 120 hojas (120 × 108 ms ≈ 13 s en un hilo). Al dibujar, la imagen de la hoja activa es la del lienzo, como hoy; al salir de ella, se sube a su textura (1,6 ms).
5. **Las texturas van en BGRA** para el MVP. Si en la laptop (gráficos integrados, memoria compartida) 857 MB pesan demasiado, la alternativa medida es el tono de 8 bits con la paleta en el shader (214 MB); con los lápices de color serían más canales.

## Recorte del medio (decidido después de la corrida)

Lo que más pesa en una hoja no es el grafito: de los 4 planos de cada tile, 3 son papel trabajado (bruñido, deformación, daño de fibra), que sirven para ilustración y no para un rough. Para Mesa de animación, y solo para ella, se recortan:

- **Fuera:** bruñido, punta seca y su deformación, daño de fibra, desgaste y afilado (la línea tiene que ser la misma en la hoja 1 y en la 40). Queda el costado, apagado por defecto: no ocupa memoria.
- **300 dpi en vez de 600:** la celda pasa de 42 a 85 µm; cada píxel en pantalla cubre unas 2 celdas por lado en vez de 4, así que la línea se ve igual hasta un zoom de ~200 %. Divide por 4 el relieve y los tiles.
- **2 planos por tile:** grafito y color (los cuatro lápices de color comparten un plano, HU-99).

Estimación con los números medidos (a confirmar en HU-82): una hoja al 26 % pasa de 72 MB a ≈ 9 MB (÷ 4 por resolución, ÷ 2 por planos); 120 hojas ≈ 1,1 GB; el relieve, 36 MB. Cargar una hoja en la simulación baja de 14 ms a menos de 4 ms. Con esto **toda la pila entra en RAM sin comprimir** y desaparecen el caché comprimido y la carga en segundo plano del punto 2 de la decisión: cambiar la hoja activa es cambiar de conjunto de tiles. El disco (HU-85) guarda comprimido, pero no se descomprime durante el uso.

## Qué cambia en las historias

- **HU-82 (pila en el lienzo):** separar en `drymedia` el relieve compartido del conjunto de tiles por hoja; perfil de medio por app (300 dpi y 2 planos para Mesa de animación); todas las hojas en RAM; deshacer por hoja (la historia de deshacer acompaña al conjunto de tiles).
- **HU-83 (tinte):** el render compone texturas de la GPU; nunca copia píxeles de la CPU durante el flip.
- **HU-85 (formato de toma):** cada hoja se guarda como estado comprimido más su imagen (o su tono), para abrir rápido.
- **HU-97, HU-98, HU-99:** papel sin trabajar, grafito sin desgaste ni punta seca, colores en un plano compartido.
- **RNF-02:** estado de todas las hojas en RAM (≈ 1,1 GB) más sus texturas en la GPU (≈ 7 MB por hoja).

## Falta medir

- La laptop (Iris Xe con memoria compartida): crear 120 texturas y el tiempo de `CopyResource`. El exe del spike corre solo, sin instalar: `build\release\mesa-flip.exe` con sus DLL de Qt.
- Un rough real dibujado a mano en vez de trazos repetidos, cuando exista la app.

## Salida de la corrida

```text
1. Papel 7680 × 4800 celdas (120 × 75 tiles)
  crear el papel (relieve y crestas): 249 ms, +143 MB de memoria
  tiles tocados: 193 de 9000 (2.1 %), 193 con planos además del depósito
  hoja guardada como tiles (4 planos): 6.0 MB; solo depósito: 1.5 MB
1b. Coberturas mayores (los mismos trazos, desplazados)
  11 % de tiles:  32.3 MB por hoja,  3871 MB las 120 hojas
  cargar una hoja al 11 % en la simulación                  mediana    6.670 ms   máx   11.179 ms
    comprimir la hoja (qCompress nivel 1)                    mediana  190.242 ms
    descomprimir la hoja                                     mediana   62.117 ms
    comprimida:   3.6 MB por hoja (9 : 1),   435 MB las 120 hojas
  26 % de tiles:  72.0 MB por hoja,  8645 MB las 120 hojas
  cargar una hoja al 26 % en la simulación                  mediana   14.062 ms   máx   22.334 ms
    comprimir la hoja (qCompress nivel 1)                    mediana  460.270 ms
    descomprimir la hoja                                     mediana  145.245 ms
    comprimida:   9.2 MB por hoja (8 : 1),  1100 MB las 120 hojas
  50 % de tiles: 141.3 MB por hoja, 16956 MB las 120 hojas
  cargar una hoja al 50 % en la simulación                  mediana   41.729 ms   máx   58.571 ms
    comprimir la hoja (qCompress nivel 1)                    mediana 1318.753 ms
    descomprimir la hoja                                     mediana  472.803 ms
    comprimida:  26.6 MB por hoja (5 : 1),  3191 MB las 120 hojas
3. renderTone de la hoja (un hilo)                          mediana  107.713 ms
4. Imagen por hoja: 7.1 MB en BGRA, 1.8 MB en tono de 8 bits
  copiar la imagen BGRA guardada                             mediana    0.538 ms
  reconstruir BGRA desde el tono de 8 bits (tabla)           mediana    3.118 ms
5. GPU
  subir la imagen BGRA (UpdateSubresource)                   mediana    1.623 ms   máx    7.192 ms
  texturas creadas en la GPU: 120 de 120 (857 MB)
  copiar una hoja que ya está en la GPU (CopyResource)       mediana    0.158 ms   máx    0.854 ms
```

## Hecho en HU-82

- `drymedia::TileSet`: el depósito de una hoja, separado del papel. `Paper::swapTiles` cambia de hoja sin copiar tiles; el relieve es uno solo (el del papel). El hash de referencia de Cartuchera (`tst_pencil`, `determinismoYRutas`) no cambió.
- `lienzo::SheetStack`: la pila (un conjunto de tiles y un deshacer por hoja), junto al papel en el `Lienzo`, así sobrevive a recrear la simulación al calibrar (F9); como efecto lateral, el deshacer ya no se pierde al calibrar. La cambia solo el hilo de simulación, con pedidos que se atienden después de las muestras que llegaron antes.
- Activar una hoja repinta la hoja entera repartida entre los núcleos: **30,6 ms** en la desktop (4 núcleos; en un hilo, 107 ms). Lo de afuera de la hoja no se repinta.
- Latencia del trazo sin cambios: muestra → vsync mediana 11,7 ms, p95 15,3 ms, 0 vsyncs perdidos (`cartuchera --bench`).
