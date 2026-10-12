# Ejercicios de animación · Arquitectura

Ejercicios de animación corre sobre el lienzo de `libs/lienzo` con la pila de hojas de la Mesa (`sheets = true`) y sobre los ejercicios de `libs/exercises`. La app (`apps/ejerciciosanimacion/src/main.cpp`) solo junta las piezas: el lienzo, la sesión de pila, el menú F4 y el panel F2. Las teclas comunes están en `lienzo/Lienzo.h`, y las de la app en [diseno.md](../diseno.md).

## Decisiones

- **El medio de la Mesa:** papel de animación, perfil recortado, grafito HB y los lápices de color, con flip, mesa de luz, zoom, pan y deshacer. La pila es la de la Mesa, pero sin toma: vive en memoria y no se guarda ([idea](idea.md)).
- **Una guía por hoja, en el render** (HU-119): el ghost de cada hoja no queda en la hoja. Así G lo oculta sin repintar, la mesa de luz no muestra los de las vecinas y repetir es armar la pila otra vez.
- **Los ejercicios en una biblioteca** (HU-118): `libs/exercises` la comparten Ejercicios de trazos y Ejercicios de animación. Las bibliotecas de abajo (`lienzo`, `appkit`) no saben de ejercicios.
- **Enter es "siguiente"**, porque → es el flip.
- **Instalador:** el mismo de Trazos, con su ícono (mesa "Íconos") y su acceso en el menú Inicio (HU-124).

## Las piezas

```text
apps/ejerciciosanimacion   main.cpp: Lienzo + StackSession + menú F4 + panel F2 + config
libs/exercises             StackExercise, SameOnEverySheet, StrokeStacks, Turntable, PrismaQueGira, CilindroQueGira, ConoQueGira, CabezaQueGira, StackSession
libs/lienzo                resetSheets, setSheetGuide, setSheetGuidesVisible, setPanelHeading
libs/appkit                Param y ParamForm, MenuOverlay, SidePanel, Config, Shortcuts, Theme
```

### Ejercicio de pila (`StackExercise`, HU-121)

Es como `Exercise`, pero `generate(params, seed, zone)` devuelve una guía y una geometría ideal **por hoja** (`StackGenerated::sheets`). Todo ejercicio de pila tiene el parámetro `hojas` (de 2 a 24, `sheetsParam`). Es determinista (RNF-03): con la misma semilla y los mismos parámetros, salen las mismas guías.

- **`SameOnEverySheet`** (HU-122): lleva un ejercicio de Ejercicios a la pila con la misma guía en todas las hojas. Conserva su id, su título, su grupo y sus parámetros, más las hojas.
- **`StrokeStacks`** (HU-122): los 11 trazos de Ejercicios que tienen sentido en pila (líneas, curvas, elipses y perspectiva; escritura, ghosting y presión no), con 6 hojas por defecto.
- **`PrismaQueGira`** (HU-123), **`CilindroQueGira`** (HU-130), **`ConoQueGira`** y **`CabezaQueGira`** (HU-131): las formas que giran, propias de la app, más abajo.

### La sesión de pila (`StackSession`, HU-121)

Es la de Ejercicios llevada a la pila. Habla con el lienzo por una interfaz chica (`StackCanvas`: `resetSheets`, `sheetSize`, `pixelsPerMm`, `setSheetGuide`, `setGuidesVisible`), así las pruebas la manejan con un lienzo de mentira (`tst_stack_session`).

- **Enter (`next`):** otra semilla (en modo mixto, otro ejercicio de los habilitados) y arma la pila.
- **R (`repeat`):** la misma semilla y los mismos parámetros, con la pila en blanco.
- **G (`toggleGuides`):** oculta y muestra las guías de todas las hojas, sin armar otra pila.
- **Armar la pila:** `resetSheets(hojas)` y después `generate` con la zona de la hoja entera. Cada guía va a su hoja con `setSheetGuide`.
- **Los parámetros** rigen desde el ejercicio siguiente, como en Ejercicios, y quedan en config: el ejercicio, los parámetros de cada uno, el modo mixto y los ejercicios que entran en él.

### En el lienzo

La guía por hoja (`setSheetGuide`, textura por id en el render, capa `SheetPictureLayer`) y la pila nueva (`resetSheets` y `Simulation::stackReplaced`, sin crear otra simulación) están en la [arquitectura de la Mesa](../mesa-de-animacion/arquitectura.md#la-guía-por-hoja-hu-119-ejercicios-de-animación), porque viven en `libs/lienzo`. El panel de hojas dice "EJERCICIO" y el título en vez de la toma (`setPanelHeading`, HU-120 y HU-121).

## Lo común de las formas que giran (`Turntable`, HU-130)

Las cuatro formas comparten la cámara, los parámetros y el ghost (`libs/exercises/Turntable`):

- **`giro::place` y `giro::project`:** el giro sobre el eje, la inclinación de la cámara y la perspectiva.
- **`giro::facesCamera`:** si una superficie mira a la cámara.
- **`giro::frame`:** el ojo y la escala según la perspectiva y el tamaño, con el ajuste a la hoja.
- **`giro::Ghost`:** pinta los trazos visibles llenos y los ocultos punteados, pone el ángulo y guarda la geometría ideal.
- **`TurningExercise`:** el grupo "Volumen", las claves y los parámetros comunes.

Cada forma solo sabe sus medidas, qué se ve y qué se dibuja.

## El prisma que gira (HU-123)

`libs/exercises/PrismaQueGira`. Un prisma de medidas al azar gira sobre uno de sus ejes, un paso por hoja. En cada hoja va su wireframe con el giro de esa hoja.

- **Lo que sale de la semilla (`prisma::draw`):**
  - las medidas: una es 1 y las otras dos están entre 1 y 3, en cualquier orden, normalizadas a diagonal 1;
  - el eje, si es "al azar";
  - el giro de la primera hoja, un múltiplo de 5°, para que no arranque siempre de frente.
- **La proyección (`prisma::project`):**
  - Las 8 esquinas giran sobre el eje: vertical (y), horizontal (x) o profundidad (z).
  - Después se inclinan 22° (`kElevationDeg`) para que la cámara mire un poco desde arriba y se vea la tapa.
  - Al final se proyectan en perspectiva, con el ojo a una distancia `eye` del centro: `p · eye / (eye − z)`, que deja el plano del centro en su tamaño real.
- **La perspectiva:** el parámetro va de 0 a 1 y fija `eye` entre 32,5 radios (casi isométrica) y 2,5 radios (fuga fuerte). El radio es media diagonal.
- **Las aristas ocultas:** una arista se ve si al menos una de las dos caras que la tocan mira a la cámara, es decir, si el producto de la normal por el vector de la cara al ojo es positivo. Si no, está oculta. Se dibujan punteadas o no se dibujan, según el parámetro. La geometría ideal tiene siempre las 12.
- **El tamaño:** es la diagonal en mm (la hoja está a tamaño real). Si con el ojo cerca la esfera que envuelve al prisma no entra en el 84 % del lado corto de la hoja, se achica.
- **El dibujo** (mesa "Prisma que gira"):
  - ghost en gris tinta `kMarcaHoja` al 50 % (`kGhostOpacidad`);
  - aristas ocultas al 40 %, más finas y punteadas;
  - el ángulo de la hoja abajo a la derecha, en IBM Plex Mono, porque arriba a la derecha va el número de la hoja.

## El cilindro que gira (HU-130)

`libs/exercises/CilindroQueGira`. Como el prisma, pero un cilindro de radio y largo al azar.

- **Lo que sale de la semilla (`cilindro::draw`):** el largo sobre el diámetro, entre 1:3 y 3:1 (parejo en escala logarítmica), dentro de una esfera de radio 1; el eje, si es al azar, y el giro de la primera hoja.
- **El eje por defecto es el horizontal:** el cilindro da vueltas de punta y las tapas cambian de elipse. Sobre el vertical gira sobre sí mismo, y solo se mueve la costura.
- **Las tapas:** dos círculos de 96 tramos, proyectados uno por uno, así la perspectiva queda exacta. Un tramo se ve si lo ve su tapa o el costado en ese punto.
- **La silueta:** las dos generatrices de canto a la cámara. La condición es n(t)·(ojo − p) = 0, y como n(t)·p = r en todo el largo, queda A cos t + B sin t = r, que tiene solución exacta. Si se lo ve justo de punta, no hay silueta.
- **La costura:** la generatriz que a 0° mira a la cámara, más una marca de ahí hacia el centro de la tapa de arriba. Sin ella, el giro sobre su propio eje no se ve. Un parámetro la saca.

## El cono y la cabeza que giran (HU-131)

**El cono** (`ConoQueGira`) es como el cilindro, con una sola tapa:

- **La silueta:** toda generatriz pasa por el vértice, así que el costado en t mira a la cámara si n(t)·(ojo − vértice) > 0. Con n(t) = h·(cos t, 0, sin t) + r·(0, 1, 0) queda A cos t + B sin t + C > 0, y la silueta es su borde, con solución exacta. No hay silueta cuando el vértice cae adentro de la base, visto de punta.
- **La base** se ve donde la ve su cara o el costado.
- **La costura** es una generatriz más una marca en la base.

**La cabeza** (`CabezaQueGira`) es la de Loomis simplificada, solo la estructura (mesa "Cono y cabeza que giran"):

- **Las piezas:** la esfera del cráneo, con el pivote en su centro; la línea media y la de las cejas, que son círculos máximos; los cortes laterales, que son círculos planos donde va la oreja; la línea de la cara, de la base de la nariz al mentón, y la mandíbula, de los cortes al mentón.
- **El contorno:** como el centro está en el eje de la cámara, los rayos tangentes tocan la esfera en un círculo, que proyectado es otro círculo de radio R·ojo/√(ojo² − R²).
- **Qué se ve:**
  - de la esfera, lo que mira a la cámara;
  - un corte, si su plano mira a la cámara;
  - la cara y la mandíbula, según planos aproximados.
- **Lo que varía con la semilla:** poco, solo el corte y el largo de la cara.
- **La costura** es la línea media.

## Rendimiento

| Requisito | Medido |
| --- | --- |
| RNF-01: flip con guía en cada hoja | Máximo 16 a 18 ms en el smoke (22 flips por 12 hojas). Lo que pasa de 18 ms es jitter del sistema; el smoke tolera uno cada 30 y ninguno de 20 ms o más (`FlipsOk`) |
| RNF-04: de la tecla a la pila lista, 12 hojas | 120 a 175 ms ("Pila lista" en el log; el prisma tarda un poco más que la recta) |

## Pruebas

- **`tst_stack_session`:** Enter, R, G, los parámetros, el modo mixto y los trazos en pila, con un lienzo de mentira.
- **`tst_prisma`:**
  - la proyección a 0° de lejos y a 90° sobre cada eje (igual al prisma con dos medidas cambiadas);
  - que la perspectiva agranda lo de adelante;
  - que las aristas ocultas son las de atrás: 5 de frente y las 3 de la esquina del fondo a 45°;
  - que cada hoja suma un paso;
  - que la misma semilla da el mismo prisma;
  - que todo queda dentro de la hoja.
- **`tst_cilindro`:**
  - la tapa de lejos es una elipse de proporción sen 22°;
  - la silueta deja todo el cilindro de un lado, y de punta no hay;
  - lo oculto es la parte de atrás de la tapa de abajo, con el límite exacto;
  - sobre su propio eje solo se mueve la costura;
  - cada hoja suma un paso;
  - la semilla;
  - que entra en la hoja.
- **`tst_cono`:** la silueta va al vértice y deja todo el cono de un lado (sin silueta, el vértice cae adentro de la base); lo oculto de la base; la base de frente; el paso por hoja, la costura, la semilla y que entra en la hoja.
- **`tst_cabeza`:** el contorno exacto con todo adentro; de frente, la línea media vertical, todo simétrico y la mitad de adelante de las cejas; de costado, el corte de frente; el paso por hoja, la semilla y que entra en la hoja.
- **`tst_lienzo`:** la guía de la hoja que se ve (`guiaDeLaHojaQueSeVe`), la pila nueva (`pilaNueva`) y el panel sin toma (`panelSinToma`).
- **Prueba de la app real** (`scripts/smoke.ps1`): arranca en el prisma con 12 hojas y verifica:
  - Ctrl+, abre los atajos;
  - Enter, R y G arman la pila en menos de 300 ms;
  - el flip respeta RNF-01 y L prende y apaga la mesa de luz;
  - F4 elige otro ejercicio (hoy, el cilindro) y arma su pila;
  - Ctrl+Q la cierra.
