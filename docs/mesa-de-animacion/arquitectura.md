# Mesa de animación · Arquitectura

Mesa de animación corre sobre el lienzo de `libs/lienzo`, como Cartuchera. Lo que necesita la pila de hojas vive en `lienzo` y `drymedia`, no en la app (RNF-07). Las teclas comunes están en `lienzo/Lienzo.h`.

## Decisiones

- **Rough, no clean-up:** el medio es recortado (HU-112): un solo grafito (HB, HU-98), sin punta seca, afilado ni textura visual del papel. Importa más que flipear sea instantáneo y que la toma pese poco que la calidad de la línea.
- **300 dpi y grano 300** (HU-113, decisión del usuario sobre la de HU-97, 600 dpi y grano 900): una hoja ocupa ~5,4 MB en memoria y ~1 MB en disco al 30 % de tiles; 120 hojas, ~650 MB de RAM.
- **Toda la pila en memoria, y en la GPU una textura por hoja** (spike HU-81): el flip solo cambia qué textura se muestra, sin tocar el disco ni la simulación (RNF-01, menos de 16 ms).
- **Sin reproducción automática, fps ni hold:** el timing lo siente la mano al flipear ([idea](idea.md)).
- **La toma es una carpeta** legible sin la app (RNF-08) y **se guarda sola** en las pausas del lápiz; no hay "Guardar".
- **Lápices de color** (HU-99): un medio aparte del grafito (`Medium::colored`: mina de 0,7 mm, techo 44000, blandura 16), que deposita en su propio plano con el mismo contacto. Los cuatro comparten el plano y gana el último color; la cera no cambia cómo agarra el grafito encima. En pantalla: la hoja, encima los colores según su cantidad y arriba el grafito (`renderTone`). La goma borra solo lo del lápiz elegido: con el grafito, el grafito; con un color, solo ese color, 4 veces más despacio que el grafito (en las pruebas, una pasada deja ~56 % del color y ~12 % del grafito; `Medium::eraseColor`). La mesa de luz pasó a naranja y turquesa para no confundirse con el rojo y el verde. 1 es el grafito y 2 a 5 los colores; en esta app los números no giran la vista. El plano de color se suma al tile de todas las apps: la hoja activa ocupa 25 % más en memoria; las guardadas, no (solo guardan los planos que usan). El hash de una hoja sin color no cambió.
- **Deshacer común** para los trazos y las operaciones con las hojas (HU-87, HU-88); no se guarda en la toma.
- **Instalador:** el mismo de Trazos (HU-110), con su ícono y su acceso en el menú Inicio. zstd se compila dentro del exe: no agrega DLL.

## La toma en disco (HU-85)

Una toma es una carpeta que se puede copiar a otra máquina y leer sin la app (RNF-08):

```text
toma-1/
  toma.json        el papel, el orden de las hojas con su rol y cuál es la fija, la activa, la vista; lugar para el audio
  hoja-1.hoja      una hoja: su depósito y su imagen de pantalla, comprimidos con zstd
  hoja-2.hoja
  ...
```

- **Dónde está la carpeta:** `%LOCALAPPDATA%\trazos\tomas\<nombre>`, con el nombre de la última toma usada (`take` en config.json, `toma-1` la primera vez). O la que se pase con `--toma <carpeta>`, que no cambia config.json (la prueba de la app real usa una temporal).
- **Cuándo se abre y se guarda:** se abre al arrancar y se guarda sola mientras se trabaja (HU-86, más abajo).
- **El código:**
  - `lienzo/Take.h`: `saveTake` y `openTake`, y las piezas `writeSheetFile` y `writeTakeIndex`.
  - `lienzo/TakeSaver.h`: el guardado automático.
  - `drymedia::Paper`: `serializeTiles` y `deserializeTiles`, el depósito como bytes.
  - `lienzo/SheetImages.h`: las imágenes de las hojas, comprimidas.

### toma.json

```json
{
    "formato": "toma de Mesa de animación",
    "version": 1,
    "papel": { "semilla": 1, "dpi": 300, "grano": 300, "anchoCeldas": 3840, "altoCeldas": 2400 },
    "hojas": [
        { "id": 1, "archivo": "hoja-1.hoja", "rol": "key" },
        { "id": 3, "archivo": "hoja-3.hoja", "rol": "" },
        { "id": 4, "archivo": "hoja-4.hoja", "rol": "", "fija": true }
    ],
    "activa": 0,
    "vista": { "giro": 0, "zoom": 1, "pan": [0, 0] },
    "audio": { "archivo": "", "marcas": [] }
}
```

- **`hojas`:** las hojas en orden.
  - `id` no cambia al mover ni al agregar otras hojas (es el de `SheetOrder`).
  - `rol`: `key`, `breakdown` o vacío (HU-104). Va por hoja, así sigue a su hoja al reordenar; una toma sin roles abre igual.
  - La guía de campo (HU-103) tampoco es de la toma ni de la hoja: Qt la pinta una vez en una textura del tamaño de la hoja (lienzo/FieldGuide.h) y la composición la multiplica en gris tinta bajo el grafito, antes del giro. Con la guía visible se compone en cada frame (benchmark: 12,1 ms de mediana con guía, 11,3 sin ella).
  - `fija`: `true` en la hoja fija (HU-89), como mucho una; las demás no lo llevan. La hoja fija no se mueve de su lugar en la pila: el flip, Inicio, Fin, ↑ ↓ y la numeración la saltean (lienzo/FixedSheet.h). El render la pone como una capa más debajo de todas, gris grafito al 35 %, con la mesa de luz prendida o apagada y al flipear; eso compone la imagen en cada frame (benchmark: 12,1 ms de mediana con hoja fija, 11,7 sin ella).
  - `vista`: el giro en grados, el zoom (HU-101; las tomas de antes abren a 100 %) y el corrimiento (HU-102) en fracción del ancho y el alto de la hoja, así no depende de la pantalla.
  - Zoom (HU-101): la vista es una transformación completa, `pantalla = c + pan + escala · R · (imagen − c)` (lienzo/ViewRotation.h). La simulación lleva cada muestra a la hoja con la inversa (el trazo cae bajo la punta) y el render agranda la imagen de pantalla en la GPU, con filtro bilineal; al alejar promedia los texeles que cubre cada píxel, así una línea fina se aclara en vez de desaparecer. No se redibuja el papel a la resolución del zoom: más allá de 100 % la línea se ve suave (opción A, elegida por el usuario; volver a dibujar desde las celdas queda como historia aparte si hace falta). Benchmark igual con zoom (11,0 ms de mediana a 207 %).
  - La marca de la esquina (el número y el rol de la hoja que se ve) no es grafito: la pinta Qt en una textura chica (lienzo/SheetRole.h) y el render la tiñe de gris tinta encima de la hoja. Con la vista sin girar y sin mesa de luz ni flip se pinta solo su recuadro sobre la copia de siempre (`Compositor::drawMark`); si no, va dentro de la composición, antes del giro, y gira con la hoja. Benchmark igual que Cartuchera (11,3 a 11,6 ms de mediana; la composición entera en cada frame costaba 0,5 ms).
- **`activa`:** la posición de la hoja activa en `hojas`.
- **`papel`:** tiene que ser igual al del papel de la app. Una toma de otro papel no se abre.
- **`audio`:** queda vacío hasta HU-106 a HU-108.
- **Versiones:** una `version` mayor que la que conoce la app no se abre.

### hoja-<id>.hoja

Binario, little endian. Una cabecera de 60 bytes y después dos bloques comprimidos con zstd: el depósito y la imagen.

| Desde | Tipo | Campo |
| --- | --- | --- |
| 0 | char[4] | `TRZH` |
| 4 | u16 | versión del archivo (1) |
| 6 | u8 | compresión: 1 = zstd |
| 7 | u8 | reservado (0) |
| 8 | u32 × 5 | el papel: semilla, dpi, grano, ancho y alto en celdas |
| 28 | u64 | tamaño del depósito sin comprimir |
| 36 | u64 | tamaño del depósito comprimido |
| 44 | u32 × 2 | ancho y alto de la imagen en píxeles (0 × 0: no hay imagen) |
| 52 | u64 | tamaño de la imagen comprimida |
| 60 | | el depósito comprimido, y después la imagen comprimida |

**El depósito** (`Paper::serializeTiles`) está formado por:

1. `tilesX`, `tilesY` y la cantidad de tiles tocados (u32 cada uno);
2. el índice de cada tile (u32, creciente, fila por fila);
3. sus planos (u8, como `tilePlanes`: bit 1 bruñido, 2 deformación, 3 daño de fibra, 4 color);
4. los planos que usa cada tile, en ese orden: el depósito y los marcados, de 64 × 64 u16 cada uno.

En Mesa de animación (medio recortado) cada tile lleva el depósito y, si se usaron lápices de color, el plano de color (HU-99). Las tomas guardadas antes de HU-99 abren igual: no tienen el bit 4.

**El plano de color** (HU-99): cada celda, u16, guarda qué lápiz de color tiene en los 2 bits altos (0 azul, 1 rojo, 2 verde, 3 violeta) y cuánto en los 14 bajos (la escala del depósito dividida por 4); 0 = sin color.

**La imagen** es la región de la hoja en la pantalla donde se guardó, en BGRA, para no redibujarla al abrir. Al abrir se usa si coincide con el tamaño de la hoja en esa pantalla; si no (otra máquina, otra área útil), la hoja se redibuja desde su depósito.

**Compresión:** zstd nivel 1, con suma de verificación en cada bloque. Se eligió así en el [spike de compresión](spikes/toma.md), a 600 dpi. A 300 dpi y grano 300 (HU-113), con la hoja al 30 % de tiles: 0,74 MB el depósito y 0,23 MB la imagen; se guarda en 8 ms y se abre en 2,5 ms. El código de compresión de la cabecera permite cambiar de algoritmo más adelante sin romper las tomas guardadas.

### Guardar y abrir

- **Guardar:**
  - Se escriben primero las hojas y después `toma.json`, cada archivo de una vez (`QSaveFile`): un corte a mitad de camino deja la toma anterior entera.
  - Al final se borran los archivos de las hojas que ya no están.
  - Las hojas se reparten entre los núcleos.
  - La imagen de la activa es la de pantalla. La de cada una de las demás es la que dejó al salir: la simulación se la pasa a `SheetImages`, que la comprime en su hilo, sin frenar el cambio de hoja.
- **Abrir:**
  - Reemplaza la pila, sin deshacer.
  - Una hoja cuyo archivo falta o está dañado (cortado, con otra marca, otro papel, o con una suma de verificación que no coincide) queda en blanco. Se avisa en el log y en F3: "Toma: la hoja N en blanco".
  - Sin `toma.json` no abre, y la app arranca con una hoja en blanco.
- **La simulación:** `saveTake` y `openTake` la detienen mientras duran. La app usa `saveTake` solo en las pruebas; guarda con el guardado automático.
- **El deshacer** no se guarda.

### Guardado automático (HU-86)

`Lienzo::useTake(carpeta)` abre la toma, o empieza una nueva ahí, y desde ese momento la guarda sola.

- **Qué cambió:**
  - Cada hoja de `SheetStack` lleva una versión, que cambia con cada trazo, deshacer, rehacer o limpieza, y la última versión guardada.
  - Una hoja nueva no está guardada. Tampoco una que vuelve a la pila (deshacer un borrado, rehacer un insertar), porque su archivo se borró.
  - Las hojas abiertas de la toma quedan como guardadas.
- **Cuándo:** cuando el lápiz lleva 300 ms sin dibujar y no hay pedidos de la pila (`Simulation::kAutosaveDelayMs`). El lápiz en el aire no cuenta como dibujar.
- **Qué hace la simulación en esa pausa:**
  - Copia el depósito de cada hoja que cambió y, de la activa, la imagen de pantalla. La imagen de las demás ya está en `SheetImages`.
  - Pasa `toma.json` si cambió el orden, la activa o la vista.
  - La copia va de a tramos de ~0,2 ms. Se corta si el lápiz toca el papel o llega un pedido: la hoja queda sin guardar y se copia en la pausa siguiente.
- **`TakeSaver`:**
  - Comprime y escribe en su hilo, con prioridad baja para no competir con el render.
  - Escribe primero las hojas y después `toma.json`, así `toma.json` nunca nombra un archivo que todavía no existe.
  - Si llegan dos copias de la misma hoja antes de escribirla, se queda con la última. Lo mismo con `toma.json`.
  - Al escribir `toma.json` borra los archivos de las hojas que ya no están.
- **Al cerrar:**
  - La simulación pasa lo pendiente sin esperar la pausa, y `Lienzo::stop` espera a que esté escrito.
  - Es solo lo cambiado desde la última pausa (casi siempre nada), así que no depende de cuántas hojas hay. El log dice cuánto tardó, cuántas hojas se escribieron en la sesión y lo más que tardó una copia.
- **Si la app muere:** se pierde a lo sumo lo de los últimos 300 ms. Cada archivo se escribe de una vez, así que la toma en disco siempre está entera.
- **Una toma que no se puede abrir** (otro papel, de una versión más nueva, `toma.json` dañado) no se pisa: se usa la primera de al lado (`<carpeta>-2`, `-3`...) que abre, o una nueva (`takeFolder`). Se avisa en el log y en F3, y esa queda como la última toma en config.json, así el arranque siguiente la abre sin avisar.
- **Errores al escribir:** van al log y a F3 ("Toma: no se pudo guardar").

## El panel de hojas (HU-114)

Angosto (240 px a escala 1), a la derecha de la pantalla, con la toma y si está guardada, la hoja activa y el total, las miniaturas de la activa y de 3 antes y 3 después, el lápiz, la mesa de luz y el giro de la vista. Tab lo muestra y lo oculta (`sheetPanel` en config.json). Diseño: mesa "Panel de hojas" del canvas.

- **`lienzo/SheetPanel.h`:** qué muestra (`PanelInfo`, `panelSlots`), dónde va cada cosa (`panelLayout`, en píxeles del cliente con la escala de Windows) y el panel pintado con Qt (`paintPanel`). Se puede probar sin GPU.
- **`Lienzo`:**
  - Arma el `PanelInfo` en cada cambio de la pila o flip (`applyLayers`) y con un reloj de 200 ms (el estado del guardado, el lápiz, el giro).
  - Lo vuelve a pintar solo si cambió algo.
  - "guardado" quiere decir que no hay hojas sin pasar al guardado (`Simulation::unsavedSheets`) ni nada por escribir (`TakeSaver::pending`).
- **`Renderer::setPanel`:**
  - Copia la imagen del panel encima de todo, sin girar con la vista.
  - Arma cada miniatura en la GPU (un shader que promedia la región de la hoja y oscurece el grafito 3 veces), una vez por cambio: al llegar la textura nueva de esa hoja, o, la activa, desde la imagen de pantalla a lo sumo cada 250 ms mientras se dibuja.
  - Después solo copia la miniatura. Una hoja que nunca estuvo activa se ve en papel liso.
- **Costo:** nada en el hilo de la simulación. En el render, copiar unas regiones chicas por frame. Los benchmarks no cambian.

