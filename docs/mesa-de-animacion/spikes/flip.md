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

## Hecho en HU-83

- Cada hoja tiene un id estable (`SheetOrder::ids`): el render guarda la textura de cada hoja por id, no por posición.
- Al activar otra hoja, la simulación deja la imagen de la que sale (la región de la hoja, desde la CPU) en `DisplayImage::retired`; el render la sube a la textura de esa hoja. Se hace al final del lote, cuando los trazos ya están en la imagen (en el primer intento se hacía antes y, si el trazo y el pedido llegaban juntos, la hoja se guardaba sin su último trazo; lo encontró la prueba en Release).
- `lienzo::Compositor` arma la mesa de luz en la GPU: la hoja que se muestra (la activa o, para el flip, la textura de otra) con hasta 6 hojas debajo, de las que se toma solo el grafito, teñido y multiplicado. **6 hojas a 1734 × 1080: 0,72 ms de GPU** (GTX 960). Sin capas ni otra hoja, el render es el de siempre.
- Latencia del trazo sin cambios, medida contra el commit anterior alternando 3 veces: mediana 11,6–11,8 ms en las dos versiones, 0 o 1 vsync perdido por corrida en las dos.
- Pendiente conocido: al calibrar con F9 cambia el tamaño de la hoja y las texturas de las hojas no activas dejan de servir; cada hoja vuelve a tener la suya la próxima vez que está activa y sale.

## Hecho en HU-90

- ← → cambian solo la hoja que se *muestra*: el render la compone desde su textura (no se repinta nada). Al soltar la tecla esa hoja pasa a ser la activa; mientras la simulación la repinta y el render sube la imagen de pantalla de a partes (~10 frames), se sigue viendo la textura. `DisplayImage::sheet` dice de qué hoja es la imagen y `SheetSource` (en el render) elige entre la imagen, la textura o una hoja en blanco.
- Al soltar, `Lienzo` pide la hoja con `untilActive`: el render la muestra desde la textura hasta que la imagen la tiene entera y de ahí vuelve solo a la activa. Cualquier otro cambio de la pila (o deshacer) corta esa espera.
- Mientras hay una flecha apretada, el lápiz no dibuja (las muestras llegan sin contacto) y la mesa de luz no se compone. Si el lienzo pierde el foco, el flip termina como si se soltara.
- Orden entre pedidos y muestras: cada pedido de la pila lleva cuántas muestras había en la cola cuando se hizo y la simulación lo atiende justo ahí, aunque llegue en el mismo lote (antes se atendían al final del lote: soltar la flecha y apoyar el lápiz enseguida podía dejar el trazo en la hoja anterior).
- Medido en la desktop (smoke, 32 flips): de la tecla al Present, de 0,4 a 16,7 ms, repartidos en un frame como se espera. Latencia del trazo sin cambios (`cartuchera --bench`: mediana 11,8 ms, p95 15,7 ms).

## Hecho en HU-91

- La mesa de luz se pide contada desde la activa (`LightTable`: hojas hacia atrás y hacia adelante, cada una con color y opacidad) y `Lienzo` la recalcula cada vez que cambia la pila: los cambios de la interfaz al momento y los que hace la simulación sola (deshacer) por un aviso (`Simulation::setOnSheetState`).
- Mesa de animación: L la prende y apaga y queda en config (`lightTable`, prendida por defecto); 2 hojas atrás (60 % y 30 %) en `kMesaLuzAnterior` y 1 adelante (60 %) en `kMesaLuzSiguiente` (`Theme.h`, mesa "Mesa de luz" del canvas).
- RNF-03 medido (smoke, 32 flips): al soltar, la mesa de luz se presenta en 14,6 ms como mucho. Latencia del trazo sin cambios (mediana 11,8 ms).

## Hecho en HU-112

- Perfil de medio por app (`LienzoOptions::profile`): Mesa de animación es `Animation`, 300 dpi y sin bruñido, punta seca ni hundimiento, daño de fibra ni desgaste (sin las teclas A y E). Cartuchera y Ejercicios siguen en 600 dpi con todo y con los mismos hashes.
- Una hoja que no está activa guarda de cada tile solo los planos que usa (el papel la empaqueta al soltarla y la desempaqueta al tomarla; los kernels no cambiaron). **La hoja de Mesa de animación (3840 × 2400 celdas) con el 26 % de los tiles dibujado: 4,7 MB guardada, contra 18,7 MB con los 4 planos** (`tst_paper`, `hojaGuardadaEmpaquetada`).
- La misma línea a 300 y a 600 dpi: grafito por mm de línea 0,145 / 0,144; ancho 0,51 / 0,42 mm (la celda más grande ensancha el borde; el ajuste fino es de HU-97).
- Activar una hoja sigue en ~29 ms: el repintado depende de los píxeles de la pantalla, no de las celdas. Latencia del trazo en Mesa de animación: mediana 10,6 ms, 0 vsyncs perdidos; el lápiz, 0,046 ms por lote (en Cartuchera, 11,0 ms).

## Hecho en HU-97

- Papel de animación: color `kHojaAnimacion` (`#FAF8F2`), sin textura visual ni su atajo (no toca la textura guardada de la familia) y con el diente más bajo en la simulación (`PaperSpec::grain` = 900, de 2047). El color del papel es un dato del lienzo: lo usan la imagen, la mesa de luz (el tinte se calcula contra él) y la hoja en blanco del render.
- **Vuelta a 600 dpi.** Las láminas de comparación (`docs/mesa-de-animacion/capturas/`, `spikes/mesa-papel`) no alcanzaron para decidir; probando en la app, a 300 dpi el grano se veía grueso con cualquier profundidad, y el usuario eligió 600 dpi con grano 900. La resolución sigue siendo un dato del papel; el perfil de animación quedó en 600.
- Memoria a 600: la hoja de Mesa de animación con el 26 % dibujado, 18,4 MB guardada (73,1 MB con los 4 planos). 120 hojas ≈ 2,3 GB; 24 hojas (la salida de la Fase 1) ≈ 450 MB.
- Latencia del trazo en Mesa de animación: mediana 11,0 ms, 0 vsyncs perdidos (Cartuchera, 10,9 ms).

## Hecho en HU-98

- Un solo grafito, la HB, elegida dibujando contra la B (con una tecla provisoria que alternaba las dos en vivo, ya sacada). `LienzoOptions::fixedGrade`: la dureza viene de `medios.json` pero Mesa de animación no lo toca (sin F5, sin las teclas de calibración ni Ctrl+S, y la dureza no se guarda en config). `tiltByDefault = false`: el costado arranca apagado.

## Hecho en HU-85

- La toma en disco: `toma.json` y un archivo por hoja, con el depósito y la imagen de pantalla comprimidos con zstd 1. Formato en [arquitectura.md](../arquitectura.md) y comparación de algoritmos en [toma.md](toma.md).
- Las imágenes de las hojas que no son la activa se guardan comprimidas en RAM (`SheetImages`, ~0,3 MB cada una): las comprime un hilo propio cuando la hoja sale, sin frenar el cambio de hoja.
- Arreglado de paso: al recrear la simulación (F9), el destructor de la vieja dejaba a la pila sin el aviso `onLeave`, y desde ahí las hojas que salían no le daban su imagen al render. Ahora la vieja se destruye antes de crear la nueva.
