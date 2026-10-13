# Bocetos — La idea

13 de octubre de 2026 · Daniel

> Documento de idea: qué queremos hacer y, sobre todo, qué no. Los requerimientos y el backlog vienen después.

## Qué es

Una quinta app de la suite: **la biblioteca de mis cuadernos de bocetos**. Cartuchera es donde se dibuja; Bocetos es donde lo dibujado se guarda, se ordena y se vuelve a mirar.

Un cuaderno es un tema: "Bocetos diarios", "Estudios de gestos", "Diseño de personajes" del Proyecto xxx. Se llena de a una hoja, siempre la siguiente, dibujando en Cartuchera; cuando la hoja está terminada vuelve al cuaderno y ya no se toca, como en un cuaderno de papel. Después se lo recorre hoja por hoja con **PgUp** y **PgDn**.

**Para qué:** el hábito. Un cuaderno con fecha de inicio y de última hoja, que se va llenando, invita a volver todos los días; y recorrerlo de punta a punta muestra el progreso (o el recorrido de una idea) mejor que una carpeta de PNG sueltos.

El nombre es **Bocetos** (ejecutable `bocetos`, documentos en `docs/bocetos/`).

## Qué no es

| Fuera | Por qué |
| --- | --- |
| Un editor | Bocetos no dibuja ni retoca: eso es Cartuchera. Bocetos guarda y muestra. |
| Editar una hoja terminada | Terminar es definitivo, como la tinta en un cuaderno. Si una hoja no gusta, se dibuja otra. |
| Borrar hojas sueltas | Por lo mismo: el cuaderno es un registro honesto. Se puede borrar un cuaderno entero. |
| Puntuar, estadísticas, rachas | Fuera de alcance de la suite (`docs/alcance-v1.md`). Las fechas en la tapa alcanzan. |
| Nube, compartir, sincronizar | Los cuadernos son locales y propios. Exportar para compartir queda para más adelante. |
| Animar | Eso es la Mesa. Las hojas de un cuaderno no se flipean ni tienen mesa de luz. |

## El modelo: colecciones, cuadernos y hojas

- Una **colección** agrupa cuadernos (por ejemplo "Proyecto xxx"). Es un solo nivel: no hay colecciones dentro de colecciones. Los cuadernos sin colección van al estante **general**.
- Un **cuaderno** tiene nombre, color de tapa, papel, máximo de hojas, fecha de inicio, fecha de la última actualización y sus hojas en orden.
- Cada **hoja** está en uno de tres estados: **vacía**, **borrador** (empezada en Cartuchera, sin terminar) o **terminada** (definitiva, no editable).
- **Se llena en orden**, como un cuaderno de verdad: "dibujar" abre siempre el borrador en curso o, si no hay, la primera hoja vacía. No quedan huecos y hay a lo sumo un borrador por cuaderno.
- **El papel es del cuaderno.** Al crearlo se elige el papel de Cartuchera (color, grano y textura) y queda fijo para todas sus hojas: cada cuaderno tiene su personalidad, como uno de papel. El tamaño es el de siempre: la superficie de la tableta, apaisada, a 600 dpi.
- **Los papeles:** crema y blanco (los de hoy), **kraft** (papel madera, marrón y con fibra) y los **tonados**, tostado y gris, como el Strathmore Toned: un medio tono de fondo para dibujar las sombras con grafito y las luces con un lápiz blanco.
- **Cuando se llena, se cierra:** se puede seguir mirando pero no suma hojas. Bocetos ofrece crear "Bocetos diarios 2" con el mismo papel y en la misma colección.
- **Borrar:** solo cuadernos enteros, con confirmación. Los cuadernos se pueden renombrar y mover de colección.

## La biblioteca (la grilla)

La pantalla de inicio es una grilla de **libros apaisados**, no una tabla, agrupados en **estantes** por colección (el general primero).

Cada libro muestra en la tapa:

- el **color de tapa** y el **nombre**;
- las **hojas usadas sobre el máximo** (12/40), en el canto, como el lomo de un cuaderno que engorda;
- la **fecha de inicio** y la **de la última actualización**;
- si tiene un borrador en curso o si está cerrado (lleno).

Se navega con las flechas o tocando con el lápiz o el mouse; Enter (o tocar) abre el cuaderno. Crear un cuaderno o una colección, renombrar, mover y borrar se hacen con diálogos simples.

## El cuaderno abierto

- La hoja terminada a **pantalla completa**, sobre el fondo del tema.
- **PgDn** y **PgUp** pasan a la hoja siguiente y a la anterior; **Inicio** y **Fin** van a la primera y a la última.
- Al pie, el número de hoja sobre el total y la fecha en que se terminó.
- **+** y **−** acercan y alejan, para ver el grano de cerca.
- **Dibujar** abre en Cartuchera el borrador o la siguiente hoja vacía. Si el cuaderno está lleno, no hace nada (y lo dice).
- **Esc** vuelve a la biblioteca.

La hoja se muestra como **imagen** (el PNG que exporta Cartuchera al terminar), no con el motor de pigmento: Bocetos queda liviano, abre al instante y no necesita la tableta ni la GPU del lienzo. El estado nativo de la hoja se guarda igual, por si algún día hace falta (reexportar a otra resolución, por ejemplo).

## El viaje a Cartuchera y la vuelta

1. En Bocetos, "dibujar" lanza `cartuchera --hoja <carpeta de la hoja>` y espera a que se cierre.
2. Cartuchera abre esa hoja con el papel del cuaderno y **guarda sola mientras se dibuja**, como la Mesa guarda la toma. Muestra discretamente dónde se está dibujando: "Bocetos diarios · hoja 13".
3. **Terminar** es una tecla explícita (propuesta: `Ctrl+Enter`): Cartuchera exporta el PNG de la hoja, la marca como terminada y se cierra. Bocetos relee el cuaderno y muestra la hoja nueva.
4. **Cerrar sin terminar** deja la hoja como **borrador**: la próxima vez, "dibujar" la retoma tal como quedó. Si Cartuchera se cierra de golpe, el autoguardado ya tiene casi todo.

Las dos apps se hablan **solo por archivos**, sin sockets ni servicios: Bocetos le pasa una carpeta y, al volver, lee lo que hay. Es lo más simple y lo más robusto, y es el mismo mecanismo con el que la Mesa ya abre y guarda sus tomas (`--toma <carpeta>`).

**Cartuchera sola sigue igual.** Abierta desde su ícono es la hoja libre de siempre, que no guarda. Solo cuando la abre Bocetos con `--hoja` se activan el guardado, el papel del cuaderno y la tecla de terminar.

## Los archivos

Siguen las reglas de la Mesa: el formato nativo es el estado de la simulación, se guarda solo, sin Ctrl+S, y el PNG es la imagen para mirar.

```
Bocetos/
  bocetos-diarios/
    cuaderno.json        nombre, colección, color, papel, máximo de hojas, fechas, versión del formato
    hoja-0001/           una toma de una sola hoja (libs/lienzo/Take.h)
      toma.json
      hoja-<id>.hoja     estado nativo (zstd)
      terminada.png      la imagen que muestra Bocetos (solo si está terminada)
    hoja-0002/
    ...
```

- La **colección es un campo** de `cuaderno.json`, no una carpeta: mover un cuaderno de estante es cambiar un dato, sin mover archivos.
- **Dónde viven:** se propone `Documentos\Trazos\Bocetos` y no la carpeta de datos de la suite (`%LOCALAPPDATA%\trazos`), porque son dibujos del usuario, no datos de la app: así entran en sus backups y se encuentran a mano. A confirmar.

## Atajos (provisorios)

**Biblioteca**

| Tecla | Qué hace |
| --- | --- |
| Flechas | Moverse entre cuadernos (y de estante) |
| `Enter` / tocar | Abrir el cuaderno |
| `Ctrl+N` | Cuaderno nuevo |
| `Ctrl+Shift+N` | Colección nueva |
| `F2` | Renombrar / cambiar color |
| `Ctrl+M` | Mover a otra colección |
| `Supr` | Borrar el cuaderno (con confirmación) |

**Cuaderno abierto**

| Tecla | Qué hace |
| --- | --- |
| `PgDn` / `PgUp` | Hoja siguiente / anterior |
| `Inicio` / `Fin` | Primera / última hoja |
| `+` `−` | Zoom |
| `D` | Dibujar: abre en Cartuchera el borrador o la siguiente hoja |
| `Esc` | Volver a la biblioteca |

**Cartuchera abierta desde un cuaderno**

| Tecla | Qué hace |
| --- | --- |
| `Ctrl+Enter` | Terminar la hoja: vuelve al cuaderno, ya no se edita |
| Cerrar | Deja la hoja como borrador |

Como en todas las apps: Ctrl+, muestra los atajos y Ctrl+Q sale.

## Ideas abiertas

- ¿Dónde viven los cuadernos? Propuesta: `Documentos\Trazos\Bocetos`.
- ¿Qué teclas son "dibujar" y "terminar"? Propuesta: `D` y `Ctrl+Enter`.
- ¿Cómo se ordenan los cuadernos dentro de un estante: por última actualización, por nombre o a mano?
- ¿Miniaturas de las últimas hojas en el canto o al pasar por encima de un libro?
- Exportar, para más adelante: el PNG de una hoja donde se elija, el cuaderno entero como PDF (con la tapa).
- **Un lápiz blanco en Cartuchera:** el kraft y los tonados lucen con las luces en blanco, y hoy Cartuchera tiene grafito y goma. Sería un medio nuevo en `libs/drymedia` (pigmento claro que cubre, no que oscurece); mientras tanto, esos papeles se usan solo con grafito.
- ¿Una portada o índice del cuaderno (título, fechas, cantidad de hojas)?
- ¿Un cuaderno lleno se puede ampliar o reabrir? Por ahora no.
- **Dibujar en Krita**, para más adelante: al dibujar, elegir si la hoja se abre en Cartuchera o en Krita. Es viable porque el viaje es por archivos:
  - **Un cuaderno es de una sola app** (decidido el 13 de octubre de 2026): se elige al crearlo, como el papel.
  - **El papel se ve igual.** Al crear un cuaderno de Krita se genera una vez la imagen del papel (color, grano y textura, a 600 dpi) con el mismo código de `libs/drymedia`, sin el render del lienzo, y se guarda como `papel.png` en el cuaderno. Cada hoja nueva es un `.kra` con esa imagen como **capa de fondo bloqueada** ("Papel") y una capa vacía encima para dibujar. El grano se ve, pero el pincel de Krita no lo "siente" como el grafito de Cartuchera: es solo la imagen.
  - **Abrir y volver:** Bocetos lanza `krita <hoja.kra>` y espera. Al volver, la imagen sale del propio `.kra`, que es un zip con un `mergedimage.png` adentro, con el papel incluido, así que en el visor se ve como cualquier otra hoja.
  - **Terminar:** Krita no tiene la tecla de Cartuchera, así que al cerrar Krita la hoja queda como borrador y se marca terminada desde Bocetos.
- Más adelante, ¿cuadernos con hojas de otras apps (una toma de la Mesa, una pila de Ejercicios de animación)?

## Qué se reutiliza y qué habría que hacer

Solo a nivel idea, para medir el tamaño.

**Se reutiliza tal cual:** `libs/appkit` (configuración, carpetas, atajos, log, tema de la suite), en Cartuchera `libs/lienzo` con las tomas (`Take.h`: `useTake`, `saveTake`, `openTake`) y la exportación a PNG (`Export.h`), los íconos (`assets/icons/make-icons.ps1`) y el instalador.

**Es nuevo:**

- **La app** (`apps/bocetos`): la biblioteca, el visor de hojas y los diálogos, en Qt Widgets pintados con QPainter y el tema de la suite. No usa el lienzo.
- **El formato del cuaderno** (`cuaderno.json` y sus hojas), con versión y pruebas de ida y vuelta.
- **El modo cuaderno de Cartuchera:** el argumento `--hoja`, el papel que viene del cuaderno, el autoguardado de la hoja, la tecla de terminar (exportar el PNG y marcarla) y el aviso de dónde se está dibujando.
- **Lanzar Cartuchera** desde Bocetos y releer el cuaderno al volver.
- **El ícono**, la entrada en `installer/trazos.iss` y en `scripts/package.ps1`.
- **El diseño** de la biblioteca y del visor en el canvas de la suite, antes de programar la parte visual.
