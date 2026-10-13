# Bocetos — Arquitectura

13 de octubre de 2026 · Daniel

Cómo está hecha la app definida en [idea.md](idea.md) y [requerimientos.md](requerimientos.md). Se completa historia por historia.

## Las piezas

| Pieza | Dónde | Qué hace |
| --- | --- | --- |
| El cuaderno | `libs/notebooks` | `cuaderno.json`, el estado de las hojas y la biblioteca (HU-132). Solo Qt Core y Gui (por `QColor`): sin widgets ni lienzo, para que la usen Bocetos y Cartuchera. |

## El formato del cuaderno (HU-132)

La biblioteca es una carpeta raíz, por defecto `Documentos\Trazos\Bocetos` (`notebooks::defaultLibraryRoot()`): son dibujos del usuario, no datos de la app, así que entran en sus backups. Adentro, una carpeta por cuaderno:

```
Bocetos/
  bocetos-diarios/
    cuaderno.json
    hoja-0001/           la toma de una sola hoja de Cartuchera (libs/lienzo/Take.h)
      toma.json
      hoja-<id>.hoja
      terminada.png      solo si la hoja está terminada
    hoja-0002/
  diseno-de-personajes/
```

- **La carpeta** sale del nombre (`folderName`): minúsculas, sin acentos y con guiones, "Diseño de personajes" → `diseno-de-personajes`. Si choca, `-2`, `-3`… (`createNotebook`).
- **Las colecciones son un campo**, no carpetas: mover un cuaderno de estante es cambiar `collection`.

### `cuaderno.json`

```json
{
  "version": 1,
  "name": "Bocetos diarios",
  "collection": "",
  "cover": "#a9533d",
  "paper": { "preset": "kraft", "color": "#b98f5e", "grain": 2047, "texture": 35 },
  "maxSheets": 40,
  "started": "2026-10-01",
  "updated": "2026-10-12"
}
```

- `collection` vacía es el estante general.
- **El papel** guarda de qué papel partió (`preset`) y los valores reales: `color`, `grain` (`drymedia::PaperSpec::grain`, 0 a 2047) y `texture` (la textura visual del lienzo, 0 a 100). Mandan los valores: si un papel de partida se ajusta, los cuadernos que ya existen no cambian. Los de partida (`paperPresets()`) son crema, blanco, kraft, tonado tostado y tonado gris.
- **La versión** (`kNotebookVersion`): uno de una versión más nueva no se abre (`LoadError::NewerVersion`, queda en el log); uno viejo se lee con la nueva.
- Se escribe con `QSaveFile`: entero o nada.

### El estado de las hojas

No se guarda en el json: sale de las carpetas, así no hay dos fuentes que puedan contradecirse.

| Estado | Carpeta `hoja-NNNN/` |
| --- | --- |
| Vacía | no existe |
| Borrador | existe, sin `terminada.png` |
| Terminada | tiene `terminada.png` |

**La siguiente hoja** (`nextSheet`) es la primera que no está terminada: el borrador o la primera vacía, porque el cuaderno se llena en orden. Ninguna si está lleno.

## Pruebas

`tst_notebooks`: ida y vuelta del json, el papel guardado manda sobre el de partida, versión nueva, ausente o dañado, los tres estados de hoja, la siguiente hoja (vacía, con borrador, lleno), nombres de carpeta (acentos, signos, vacío), crear con nombres que chocan y listar salteando lo que no es un cuaderno.
