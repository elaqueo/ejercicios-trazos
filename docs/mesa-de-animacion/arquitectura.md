# Mesa de animación · Arquitectura

Mesa de animación corre sobre el lienzo de `libs/lienzo`, como Cartuchera. Lo que necesita la pila de hojas vive en `lienzo` y `drymedia`, no en la app (RNF-07). Las teclas comunes están en `lienzo/Lienzo.h`.

## La toma en disco (HU-85)

Una toma es una carpeta que se puede copiar a otra máquina y leer sin la app (RNF-08):

```text
toma-1/
  toma.json        el papel, el orden de las hojas, la activa, la vista; lugar para roles y audio
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
    "papel": { "semilla": 1, "dpi": 600, "grano": 900, "anchoCeldas": 7680, "altoCeldas": 4800 },
    "hojas": [
        { "id": 1, "archivo": "hoja-1.hoja", "rol": "" },
        { "id": 3, "archivo": "hoja-3.hoja", "rol": "" }
    ],
    "activa": 0,
    "vista": { "giro": 0 },
    "audio": { "archivo": "", "marcas": [] }
}
```

- **`hojas`:** las hojas en orden.
  - `id` no cambia al mover ni al agregar otras hojas (es el de `SheetOrder`).
  - `rol` queda vacío hasta HU-104 (key o breakdown).
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
3. sus planos (u8, como `tilePlanes`: bit 1 bruñido, 2 deformación, 3 daño de fibra);
4. los planos que usa cada tile, en ese orden: el depósito y los marcados, de 64 × 64 u16 cada uno.

En Mesa de animación (medio recortado) cada tile lleva solo el depósito.

**La imagen** es la región de la hoja en la pantalla donde se guardó, en BGRA, para no redibujarla al abrir. Al abrir se usa si coincide con el tamaño de la hoja en esa pantalla; si no (otra máquina, otra área útil), la hoja se redibuja desde su depósito.

**Compresión:** zstd nivel 1, con suma de verificación en cada bloque. Se eligió así en el [spike de compresión](spikes/toma.md): unos 4,5 MB por hoja al 26 % de tiles, se guarda en 39 ms y se abre en 13 ms. El código de compresión de la cabecera permite cambiar de algoritmo más adelante sin romper las tomas guardadas.

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
