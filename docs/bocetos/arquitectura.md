# Bocetos — Arquitectura

13 de octubre de 2026 · Daniel

Cómo está hecha la app definida en [idea.md](idea.md) y [requerimientos.md](requerimientos.md). Se completa historia por historia.

## Las piezas

| Pieza | Dónde | Qué hace |
| --- | --- | --- |
| El cuaderno | `libs/notebooks` | `cuaderno.json`, el estado de las hojas y la biblioteca (HU-132). Solo Qt Core y Gui (por `QColor`): sin widgets ni lienzo, para que la usen Bocetos y Cartuchera. |
| La app | `apps/bocetos` (`LibraryWindow`, `NotebookView`; `BocetosCore.h` y `SheetCache.h` en `bocetos_core`) | Qt Widgets en una ventana común, sin el lienzo (RNF-06): la ventana, los atajos y dónde están los cuadernos y Cartuchera (HU-134); la biblioteca, con los estantes y los libros (HU-135); el cuaderno abierto (HU-137). |
| Cartuchera en modo cuaderno | `apps/cartuchera` (`NotebookMode.h`), `libs/lienzo` (`SideNote.h`, `exportActiveSheet`) | Abre una hoja con `--hoja`, la guarda sola con el papel del cuaderno y la termina con Ctrl+Enter (HU-133). Detalle en [medios-secos/arquitectura.md](../medios-secos/arquitectura.md#modo-cuaderno-hu-133-bocetos). |

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
  "paper": { "preset": "kraft", "color": "#b59878", "grain": 2047, "texture": 35 },
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

## La app (HU-134)

- **Una app Qt común**: una ventana común, con bordes y redimensionable (no a pantalla completa: pedido del usuario del 13 de octubre de 2026, con HU-135), que recuerda su posición y tamaño (`bocetos.geometry`; la primera vez, centrada a tres cuartos del monitor), pintada con QPainter y el tema de la suite (`appkit/Theme.h`). No enlaza `libs/lienzo` (no hay ventana nativa ni Direct3D): muestra PNG.
- **El monitor** es el de la familia (`appkit::savedScreen`, que salió del lienzo para esto). F10 pasa al siguiente y lo guarda para todas (`appkit::nextScreen`); la ventana se muda, centrada, sin reiniciar.
- **Las teclas** van al registro de `appkit::Shortcuts` por la tecla virtual de Windows (`QKeyEvent::nativeVirtualKey`), como en el lienzo; Ctrl+, abre la página de atajos de siempre y Ctrl+Q sale.
- **La biblioteca** es la de `--biblioteca <carpeta>` (la prueba de la app real, para no tocar los cuadernos del usuario) o `Documentos\Trazos\Bocetos`. **Cartuchera** es `cartuchera.exe` junto a `bocetos.exe` (`bocetos::findCartuchera`); si no está, queda en el log y en la pantalla.
- Su log es `bocetos.log` y su sección de `config.json`, `bocetos`.

## La biblioteca (HU-135)

Como la mesa "Bocetos · biblioteca". Lo que no es dibujo está en `bocetos_core`, con pruebas:

1. **Los estantes** (`shelvesOf`): uno por colección, el general (sin colección) primero y después por nombre; adentro, por la última actualización, la más reciente primero (decisión 3).
2. **El libro** (`bookOf`): las hojas terminadas, si la siguiente es un borrador y si está lleno, de `libs/notebooks`. Las fechas de la tapa (`bookDates`): "desde 1 oct · última 12 oct", con el año solo si no es el de hoy; sin hojas, solo "desde".
3. **El acomodo** (`layoutLibrary`): tapas de 232 × 148 px con 40 entre una y otra, en filas de lo que entre en el ancho (en el UltraWide, 8); un estante con más libros sigue en la fila de abajo. El nombre del estante, 40 px arriba de las tapas; la línea de la tabla, 194 px debajo del borde de arriba de su última fila; el estante siguiente, 52 más abajo.
4. **La navegación** (`moveSelection`): ← → el libro anterior o siguiente, de fila en fila y de estante en estante; ↑ ↓ la fila de arriba o de abajo en la columna más cercana. En los bordes se queda. Si no entra todo, la vista se corre para que el elegido se vea entero.
5. **El dibujo** (`LibraryWindow`): la tapa con su color, el lomo más oscuro, el nombre (hasta dos líneas), la marca (borrador en ámbar, lleno, sin hojas) y las fechas; debajo, el canto, que engorda de 3 a 12 px con las hojas usadas, y la cuenta "12/40"; el elegido con el marco ámbar. Sin cuadernos, "Todavía no hay cuadernos · Ctrl+N crea el primero".
6. **Abrir**: Enter, o tocar el libro (el lápiz llega como mouse), lo elige y abre el cuaderno (HU-137). El último elegido queda en `config.json` (`bocetos.selected`, la carpeta del cuaderno).
7. **RNF-01**: el log dice cuánto tardó en leer la biblioteca; la prueba de la app real lo mide con 30 cuadernos.

## Crear y administrar (HU-136)

- **Ctrl+N** abre "Cuaderno nuevo" (`NotebookDialog`, mesa "Bocetos · cuaderno nuevo"): un panel con widgets de Qt sobre la biblioteca atenuada, dentro de la ventana. Nombre, colección (desplegable editable: escribir una nueva la crea), hojas (1 a 200), tapa (`theme::kTapas`, seis tonos) y papel (desplegable con la muestra de cada uno; grano y textura arrancan en los del papel de partida; la muestra grande es ruido sobre el color, una aproximación: no es la simulación de Cartuchera). Enter crea (`notebooks::createNotebook`, empezado y actualizado hoy) y lo deja elegido; Esc cancela.
- **Seguir en uno nuevo**: si el elegido está lleno, Ctrl+N viene con "Nombre 2" (`continuedName`), el mismo papel, tapa, colección y hojas (`draftFor`); si no, la colección del elegido.
- **F2** abre el mismo panel para editar el nombre, la colección y la tapa (el papel y las hojas se ven deshabilitados: las hojas ya dibujadas son de ese papel). **Ctrl+M**, igual con el foco en la colección. La carpeta del cuaderno no se renombra: el nombre vive en `cuaderno.json`.
- **No hay Ctrl+Shift+N**: una colección es un campo del cuaderno, así que una vacía no tendría estante; se crea escribiéndola en el desplegable (decidido el 13 de octubre de 2026).
- **Supr** pide la confirmación de la suite (`appkit::ConfirmOverlay`: "¿Borrar «X»? Tiene N hojas terminadas") y manda la carpeta entera a la **Papelera de reciclaje** (`QFile::moveToTrash`), no la borra para siempre. Queda elegido el vecino.
- Mientras el panel o la confirmación están abiertos, las teclas son de ellos: la biblioteca de atrás no se mueve.

## El cuaderno abierto (HU-137)

Como la mesa "Bocetos · cuaderno abierto". `NotebookView` va encima de la biblioteca, dentro de la misma ventana; Esc lo esconde y queda elegido el mismo libro.

1. **Las páginas** (`pagesOf`): las hojas terminadas, en orden (se llenan en orden, así que son las primeras), con la fecha de su `terminada.png`; al final, la siguiente (borrador o en blanco) como aviso, con el papel del cuaderno. Un cuaderno lleno no tiene aviso. Abre en la última terminada; sin terminadas, en la hoja 1 en blanco (`openingPage`).
2. **La vista** (`fitScale`, `sheetRect`, `clampPan`, `viewerArea`): la hoja ajustada al área, que es la ventana menos la barra del cuaderno (300 px, vertical a la derecha, HU-144) y 24 px de margen. La barra lleva arriba el cuaderno, la hoja, su fecha y lo usado, y abajo las teclas, del registro; el log de "abierto" anota el área y dónde empieza la barra. + y − acercan y alejan hacia el centro en pasos de ×1,5, desde ajustada hasta un píxel de la hoja por píxel de pantalla (`maxZoom`). Arrastrar con el mouse o el lápiz corre la hoja acercada, sin dejar ver fondo de un lado en el eje en que es más grande que el área. Pasar de hoja vuelve a ajustarla.
3. **La carga** (`SheetCache`, RNF-02): cada `terminada.png` es de 600 dpi (7680 × 4800) y leerla lleva unos 250 ms, más que los 100 que pide RNF-02. Por eso se lee en segundo plano (`QThreadPool` de dos hilos), achicada al tamaño del monitor (`QImageReader::setScaledSize`), y se precargan la siguiente, la anterior y la de después; lo demás se suelta. Acercada más de lo que da la achicada, se lee también la entera, solo la que se mira (unos 150 MB); mientras tanto se ve la achicada agrandada. Al volver a la biblioteca se suelta todo.
4. **El log**: "Cuaderno: hoja 8 en 22 ms (precargada)" o "(leída en 260 ms)", desde la tecla hasta que la hoja se pinta.
5. **Las teclas** van al registro como las de la biblioteca (grupo "Cuaderno"); cada acción mira si el cuaderno está abierto. La ventana conserva el foco: el visor solo recibe el mouse.

## El viaje a Cartuchera (HU-133)

Bocetos lanza `cartuchera --hoja <cuaderno>/hoja-NNNN` y espera; se hablan solo por archivos.

**Desde Bocetos (HU-138).** **D**, en la biblioteca (el libro elegido) o en el cuaderno abierto, decide con `drawPlan`: lanzar la siguiente hoja (el borrador o la primera vacía), o, si el cuaderno está lleno, abrir "Cuaderno nuevo" con la propuesta de seguir en uno nuevo; sin `cartuchera.exe`, un aviso. Lanza con `QProcess` y espera sin bloquear: el cartel `Notice` ("Dibujando en Cartuchera · Bocetos diarios · hoja 13") tapa la ventana y las teclas no hacen nada; cerrar Bocetos se rechaza (mataría a Cartuchera con la hoja abierta). Al volver, `drawResult` mira la carpeta de la hoja y el código de salida: terminada (la muestra en el cuaderno abierto, venga de la biblioteca o del cuaderno), en borrador o sin tocar (vuelve a donde estaba, con la biblioteca releída), no la quiso abrir (código 2: aviso, el motivo en `cartuchera.log`) o se cerró de golpe (aviso: el borrador quedó guardado). Los avisos se van solos a los 6 s o con cualquier tecla.

1. Cartuchera valida la hoja (la siguiente, sin terminar) y la abre como una toma de una sola hoja con el papel del cuaderno: se guarda sola, así que la carpeta aparece con el primer trazo (hasta ahí la hoja sigue vacía).
2. **Terminar** escribe `terminada.png` de una vez (al lado y renombrado) y después `updated` en `cuaderno.json`. Desde ese momento la hoja está terminada y Cartuchera no la vuelve a abrir.
3. **Cerrar sin terminar** deja la toma sin `terminada.png`: un borrador, que es la siguiente hoja la próxima vez. Si era una hoja nueva y no tiene trazos, Cartuchera borra la carpeta al salir: sigue vacía (HU-138).

## Pruebas

`tst_notebooks`: ida y vuelta del json, el papel guardado manda sobre el de partida, versión nueva, ausente o dañado, los tres estados de hoja, la siguiente hoja (vacía, con borrador, lleno), nombres de carpeta (acentos, signos, vacío), crear con nombres que chocan y listar salteando lo que no es un cuaderno.

`tst_notebook_mode` (Cartuchera): el argumento `--hoja`, la siguiente hoja abre con su papel, lo que no abre (terminada, fuera de orden, fuera del máximo, fuera de un cuaderno) y que terminar actualiza la fecha. La prueba de la app real abre, deja un borrador, lo retoma, lo termina y verifica que no se vuelve a abrir.

`tst_bocetos`: el argumento `--biblioteca`, encontrar Cartuchera (una carpeta con ese nombre no cuenta), el orden de los estantes y los libros, lo que dice la tapa (hojas usadas, borrador, lleno, fechas), el acomodo con filas que se cortan, la navegación con las cuatro flechas, el nombre siguiente, las colecciones, lo que propone "Cuaderno nuevo" (también lleno), el cuaderno que sale del panel, lo que hace D (en blanco, con borrador, lleno, sin Cartuchera) y cómo vuelve Cartuchera (terminada, borrador, sin tocar, código 2, de golpe), las páginas del cuaderno abierto (sin hojas, con borrador, lleno), el área de la hoja con la barra, el ajuste, los pasos y el tope del zoom, el corrimiento y la carga en segundo plano (achicada, entera, soltar y un archivo que no es imagen). La prueba de la app real arranca Bocetos con una biblioteca de 30 cuadernos en 3 estantes, con hojas de tamaño real (menos de 500 ms), abre y cierra los atajos, elige con las flechas, abre con Enter en la última hoja terminada (a la izquierda de la barra), pasa de hoja con RePág y AvPág (menos de 100 ms) hasta la siguiente en blanco, va con Inicio y Fin, acerca hasta leer la hoja entera, vuelve con Esc, dibuja con D (Cartuchera cerrada sin tocar: sigue en la biblioteca; desde el cuaderno, Ctrl+Enter y Ctrl+Enter la terminan y Bocetos la muestra), prueba F11, crea un cuaderno con Ctrl+N, lo renombra con F2, lo manda a la papelera con Supr y Supr, y cierra con Ctrl+Q.
