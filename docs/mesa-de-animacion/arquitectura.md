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

- **Dónde está la carpeta:** `%LOCALAPPDATA%\trazos\tomas\toma-1`, o la que se pase con `--toma <carpeta>` (la prueba de la app real usa una temporal).
- **Cuándo se abre y se guarda:** se abre al arrancar y se guarda al cerrar. A partir de HU-86 se guarda sola, mientras se trabaja.
- **El código:**
  - `lienzo/Take.h`: `saveTake` y `openTake`.
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
- **La simulación:** guardar y abrir la detienen mientras dura. HU-86 guardará solo lo que cambió, en segundo plano.
- **El deshacer** no se guarda.
