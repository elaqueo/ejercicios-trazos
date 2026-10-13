# Diseño · "Taller nocturno"

Aprobado el 8 de octubre de 2026 (HU-37). El diseño completo, con sus 8 mesas de trabajo, está en Claude Design: [Ejercicios de trazos · Diseño](https://claude.ai/artifact/1aSCuWwcY58dXtaC1FYe51). Es privado: para que otra persona lo vea hay que compartirlo desde su menú Share.

Los valores para el código están en [`libs/appkit/include/appkit/Theme.h`](../libs/appkit/include/appkit/Theme.h), que es la fuente única.

## Idea

La interfaz es grafito y se retira: lo único claro en la pantalla es la hoja del área útil. Las guías hablan en dos voces: **azul tinta** para lo que se une o se sigue, **ámbar** para la dirección y el énfasis. Las dos se distinguen también por luminosidad, no solo por tono, y ninguna se confunde con la tinta negra del trazo.

## Color

| Token | Valor | Uso |
| --- | --- | --- |
| `fondo` | `#17191C` | Fondo de pantallas a página completa (calibración) |
| `panel` | `#22262B` | Paneles y overlays |
| `panel-alto` | `#2C3138` | Controles, teclas, ítems elevados |
| `borde` | `#3A4048` | Separa paneles; no se usan sombras |
| `texto` / `texto-2` | `#ECEEF0` / `#A7AEB6` | Texto principal y secundario |
| `fuera` | `#303338` | Fuera del área útil y esquinas al rotar |
| `hoja` | `#F5F0E6` | Off-white cálido para descansar la vista (HU-41) |
| `hojaAnimacion` | `#FAF8F2` | Papel de animación (Mesa de animación, HU-97): casi blanco, liso a la vista |
| `tinta` | `#111111` | Trazo (siempre negro) |
| `guia` | `#2563EB` | Puntos a unir, líneas de muestra, ejes |
| `guia-suave` | `#9DB8F2` | Construcción: horizonte, líneas a PF (punteada) |
| `enfasis` | `#E08A1E` | Dirección, puntos de fuga, perfil de presión, selección |
| `mesaLuzAnterior` | `#E8862A` | Mesa de luz (Mesa de animación): hojas anteriores, en naranja, al 60 % la más cercana y al 30 % la segunda (hasta HU-99 eran rojo `#C8352E`: se confundían con el lápiz rojo) |
| `mesaLuzSiguiente` | `#14A3A3` | Mesa de luz: hojas siguientes, en turquesa, ídem (hasta HU-99, verde `#2F8A4E`) |

## Tipografía y medidas

- **IBM Plex Sans** para la interfaz e **IBM Plex Mono** para teclas y valores numéricos (licencia OFL).
- Espaciado 4 · 8 · 12 · 16 · 24 · 32. Radios: paneles 12, controles 8, teclas 6. Objetivo táctil ≥ 44 px.
- Guías de 2 px; construcción de 1,5 px punteada.

## Regla de ubicación

**Todo lo que se toca con el lápiz va dentro del área útil**: la tableta solo alcanza ese rectángulo. Los paneles laterales se pegan al borde derecho del área útil (no de la pantalla) y los overlays centrados se centran en el área útil. La única excepción es la calibración, que cubre toda la pantalla porque justamente define el área.

## Decisiones por pantalla

| Pantalla | Decisión | Historia |
| --- | --- | --- |
| Lienzo | Hoja `#F5F0E6` del tamaño de la tableta, que cubre toda el área útil (HU-69); el grafito se simula sobre ella. F3 muestra la línea de latencia y herramienta | HU-41, HU-63, HU-69 |
| Guías | Punto a unir: anillo de 24 px con centro (en ámbar si es un destino, como el centro de los radiales); punto de paso: anillo chico hueco; PF: rombo ámbar, y fuera de la hoja un triángulo ámbar en el borde con rectas de ejemplo; grados y números en IBM Plex Mono ámbar; banda de presión celeste semitransparente. Van bajo el grafito (HU-64) | HU-19, HU-21 a HU-36 |
| Puntero | **Cruz abierta** (opción A): cuatro trazos finos con el centro libre | HU-38 |
| Menú (F4) | Como la mesa "Menú overlay": modo mixto destacado arriba (con cuántos ejercicios participan) y 16 ejercicios en una grilla de 3 columnas con 6 grupos (Líneas, Curvas, Elipses / Perspectiva, Escritura, Memoria y presión); el actual en ámbar; las flechas se mueven en las dos direcciones y se frenan en los bordes | HU-11, HU-20, HU-77, HU-80 |
| Panel (F2) | Panel a la derecha del área útil, pestañas Ejercicio (parámetros del actual y, en modo mixto, qué ejercicios participan) / Lápiz (mina activa y goma) / Pantalla (área útil y monitor). Los colores de la hoja quedaron fijos | HU-12, HU-20 |
| Lápices (F5) | Panel a la derecha con la imagen de la colección de grafito arriba y las diez minas con una muestra simulada; la activa resaltada. Reemplaza al selector de pinceles de libmypaint | HU-67 |
| Calibración | Pantalla oscura, indicador de pasos, marca ámbar en la esquina pedida | HU-10 |
| Confirmación | Como la mesa "Confirmación": pregunta, detalle y la tecla que confirma en ámbar; cualquier otra tecla cancela | HU-87 |
| Atajos (Ctrl+,) | Como la mesa "Atajos" (rediseñada en HU-117): título con el nombre de la app, la leyenda y cómo se cierra; grupos chicos en el orden del registro (`Shortcuts::setOrder`), repartidos solos en cuatro columnas parejas (el corte que deja la más alta lo más baja posible); teclas como chips y los gestos que no son teclas en chips de borde punteado, en su grupo "Lápiz y tableta". 1400 px de ancho, centrada en la pantalla; si no entra de alto, las filas bajan de 30 a 22 px. Sale del registro de atajos, así no se desactualiza | HU-78, HU-117 |
| Mesa de luz | Como la mesa "Mesa de luz": las hojas anteriores teñidas de naranja y las siguientes de turquesa (rojo y verde hasta HU-99: se confundían con los lápices), más tenues cuanto más lejos; se apaga mientras se flipea | HU-91 |
| Rol y número (K, Mesa de animación) | Como la mesa "Rol y número": arriba a la derecha de la hoja, a 20 px de los bordes, el número de la hoja en la pila (IBM Plex Mono 26, caja de 64 px), solo, con círculo (key) o con una raya debajo (breakdown), en gris tinta #6B7178 (el azul de las guías se confundía con el lápiz azul). No es grafito: va encima y gira con la hoja. Al flipear, el de la hoja de delante. El panel de hojas usa la misma marca en el número grande y en las miniaturas | HU-104 |
| Menú del lápiz (botón lateral, Mesa de animación) | Como la mesa "Menú del lápiz", al estilo de la paleta emergente de Krita: anillo de radio 176 px, semitransparente, centrado en la punta y corrido lo justo para entrar entero en la hoja. Adentro los cinco lápices (el activo con anillo ámbar; elegir uno cierra); afuera seis acciones en lugares fijos: mesa de luz arriba, hoja siguiente y rehacer a la derecha, vista inicial abajo, deshacer y hoja anterior a la izquierda (el menú queda abierto). Lo que está bajo la punta se resalta en ámbar. Se elige tocando con la punta, o manteniendo el botón y soltándolo sobre la opción; tocar fuera, el botón otra vez o Esc lo cierran | HU-116 |
| Selector de lápices (F5, Mesa de animación) | Como la mesa "Selector de lápices": panel de 400 px centrado sobre la hoja (al alcance del lápiz: la hoja es el área útil de la tableta), con el grafito y los cuatro colores; cada fila con su tecla, una muestra simulada (tres pasadas, para que se vea el color a 300 dpi), el nombre y para qué es; el activo con borde ámbar. Se elige con el lápiz, clic, 1 a 5 o ↑ ↓ y Enter | HU-100 |
| Lápices de color | Como la mesa "Selector de lápices": azul `#4F8FD6`, rojo `#D0392F`, verde `#3C9A52`, violeta `#8B52C4` (tokens `kLapiz*`), más tenues que el grafito y siempre bajo él; el panel de hojas muestra el activo con un cuadrito de su color | HU-99 |
| Panel de hojas (Tab) | Como la mesa "Panel de hojas": angosto (240 px) a la derecha, con la toma y si está guardada, el número grande en ámbar y el total, las miniaturas de la activa (marco ámbar) y de 3 antes y 3 después, el lápiz, la mesa de luz y el giro | HU-114 |
| Íconos | Como la mesa "Íconos": fondo gris oscuro redondeado; Ejercicios es la hoja con dos puntos azules y un trazo; Cartuchera, un lápiz ámbar; Mesa de animación, tres hojas apiladas con la pelota en grafito y, debajo, la del dibujo anterior en naranja, como la mesa de luz; Ejercicios de animación, las mismas hojas con el ghost del prisma en gris tinta y el grafito encima, siguiendo las aristas de adelante; el instalador, la hoja y el lápiz juntos. Se generan en vector con `assets/icons/make-icons.ps1` | HU-70, HU-110, HU-124 |
| Prisma que gira (Ejercicios de animación) | Como la mesa "Prisma que gira": el ghost del prisma de cada hoja en wireframe, gris tinta #6B7178 al 50 % (`kGhostOpacidad`; el azul se confunde con el lápiz azul), aristas ocultas punteadas y más finas (un parámetro las saca), el ángulo de la hoja chico abajo a la derecha en el gris del número. Va en el render, bajo el grafito, como una guía por hoja | HU-123 |
| Cilindro que gira (Ejercicios de animación) | Como la mesa "Cilindro que gira": el ghost del prisma (gris tinta al 50 %, lo oculto punteado, el ángulo abajo a la derecha); las tapas en elipses exactas, la silueta de canto y una costura (una generatriz y una marca en la tapa) que deja ver el giro; por defecto, sobre el eje horizontal | HU-130 |
| Cono y cabeza que giran (Ejercicios de animación) | Como la mesa "Cono y cabeza que giran", con el ghost del prisma. El cono: la base en elipse, la silueta del vértice y la costura; por defecto sobre el eje horizontal. La cabeza: Loomis simplificada, solo la estructura (la esfera, la línea media y la de las cejas, los cortes laterales, la línea de la cara y la mandíbula), sin nariz ni boca; por defecto sobre el eje vertical | HU-131 |
| Panel de hojas (Ejercicios de animación) | Como la variante de la mesa "Panel de hojas": en vez de la toma, "EJERCICIO" y el título del ejercicio, con "modo mixto" a la derecha si está prendido; sin estado de guardado (la pila no se guarda) | HU-120, HU-121 |
| Biblioteca (Bocetos) | Como la mesa "Bocetos · biblioteca": encabezado "Bocetos" y cuántos cuadernos en cuántos estantes; un estante por colección (el general primero) con su nombre en versalitas y la línea de la tabla; libros apaisados de 232 × 148 px: la tapa del color del cuaderno con el lomo más oscuro, el nombre, la marca (borrador en ámbar, lleno, sin hojas) y las fechas en mono; debajo, el canto que engorda con las hojas y la cuenta "12/40"; el elegido con marco ámbar; las teclas al pie | HU-135 |
| Cartuchera en modo cuaderno (Bocetos) | Como la mesa "Cartuchera en modo cuaderno": en el costado libre a la derecha de la hoja, "CUADERNO", la muestra de la tapa y el nombre, "hoja 13 de 40" en ámbar mono, las teclas (Ctrl+Enter resaltada: terminar; Ctrl+Q: queda el borrador; Z) y el papel al pie (`lienzo/SideNote.h`, lo pone el render como la franja del audio). Sin costado libre, una línea en la esquina superior izquierda de la hoja. Terminar en blanco usa la confirmación | HU-133 |

## Teclas (v1.5.2)

Todas se declaran en el registro único de atajos (HU-14); si dos acciones piden la misma tecla, la app avisa al iniciar.

| Tecla | Ejercicios | Cartuchera |
| --- | --- | --- |
| → / botón lateral del lápiz | Ejercicio siguiente | — |
| R | Repetir el ejercicio | — |
| I | Costado sí o no: apagado, la punta es siempre la vertical (HU-73) | Igual |
| E | Punta seca o mina: la punta seca hunde el papel sin grafito, para líneas blancas (HU-61) | Igual |
| A | Afilar la mina activa: la punta se gasta al dibujar, cada dureza por su cuenta (HU-62) | Igual |
| Ctrl+[ / Ctrl+] | Menos o más textura del papel, de a 10 % (0 % = hoja lisa; arranca en 20 %, común a las dos apps; HU-75) | Igual |
| Rueda de la tableta | Girar la vista libremente, sin snap: una vuelta del dedo = una vuelta de la hoja; se guarda al levantar el dedo (HU-74) | Igual |
| G | Mostrar u ocultar las guías (comparar) | — |
| F4 / F2 | Menú / panel | — |
| F5 | Selector de lápices | Selector de lápices |
| Ctrl+, | Lista de atajos; Esc o Ctrl+, la cierran (HU-78) | Igual |
| Ctrl+0 | Vista inicial: giro a 0°, zoom 100 % y centrada (HU-116, HU-101) | Igual |
| 4 / 6 / 5 | Girar la vista de a 15° / volver a 0° | Igual (desde el 10 de octubre de 2026; las durezas se eligen con F5) |
| Shift + arrastrar con el lápiz | Girar la vista libremente, sin snap (HU-79) | Igual |
| Z / Ctrl+Y | — (sin deshacer) | Deshacer / rehacer |
| `[ ]` · `, .` · `- =` | Tamaño · blandura (fuerza en la goma) · techo, de la herramienta activa | Igual |
| Ctrl+S · Ctrl+N | Guardar el lápiz · hoja nueva | Igual |
| Ctrl+Enter | — | En modo cuaderno (`--hoja`, desde Bocetos): terminar la hoja; vuelve al cuaderno y ya no se edita. Ahí no está Ctrl+N (borraría el borrador) y Ctrl+[ ] no guardan la textura (es la del cuaderno; HU-133) |
| F3 · F9 · F10 · F12 | Latencia · calibrar el área útil · monitor siguiente · captura | Igual |
| Ctrl+Q / Alt+F4 | Salir (Ctrl+Q, pedido del usuario con HU-109) | Salir |

**Mesa de animación:** las teclas del lienzo, como Cartuchera, salvo A (afilar) y E (punta seca), que no están en el medio recortado (HU-112), Ctrl+[ ] (el papel de animación no tiene textura visual, HU-97), las de calibración ([ ] , . - =) y Ctrl+S (un solo grafito, la HB, HU-98), y 4 / 6 / 5 (los números eligen el lápiz, HU-99; la vista gira con la rueda y con Shift + arrastrar); el costado (I) arranca apagado, más las de las hojas (HU-87) y el flip (HU-90). Las de las hojas se deshacen con Z.

| Tecla | Qué hace |
| --- | --- |
| ← / → | Flip: mantener la flecha muestra la hoja anterior o la siguiente; al soltarla, esa hoja queda activa. Mientras tanto el lápiz no dibuja y la mesa de luz no se ve |
| L | Mesa de luz: prende y apaga (queda guardada); 2 hojas atrás en naranja y 1 adelante en turquesa (mesa "Mesa de luz") |
| Ctrl+L | Mesa de luz solo con keys: las 2 keys anteriores y la siguiente, con los mismos colores, en vez de las vecinas; otra vez, todas las hojas. Apagada, la prende así. Queda guardado; el panel dice "solo keys" (HU-93) |
| Insert | Hoja nueva en blanco después de la activa, que pasa a ser la activa |
| K | Rol de la activa: sin rol → key → breakdown → sin rol; no se deshace (K otra vez). La copia de Ctrl+D sale sin rol (HU-104) |
| Ctrl+D | Duplicar la activa: la copia queda después y pasa a ser la activa |
| Ctrl+N | Limpiar la hoja activa |
| Supr | Borrar la hoja activa; si tiene trazos, un cartel pide Supr otra vez (mesa "Confirmación") |
| ↑ / ↓ | Mover la hoja activa un lugar antes o después en la pila (HU-88) |
| Inicio / Fin | Ir a la primera o a la última hoja, sin deshacer, como un flip (HU-115); saltean la hoja fija |
| A | Elegir el audio de referencia (WAV o MP3): se copia a la toma (HU-106) |
| P / Retroceso | Play o pausa del audio / volver al principio (HU-107) |
| M / Ctrl+M | Marca en el audio, en el cursor / borrar la más cercana; en la franja, el lápiz pone y mueve marcas y el mouse arrastra el cursor (HU-108) |
| Ctrl+E | Exportar la toma como PNG numerados en `<toma>/export`, a 300 dpi, con `toma.json` (número y rol) y el audio; la fija va aparte (`fija.png`). El panel muestra "exportando 3/12" y después "exportado" (HU-109) |
| W | Mostrar u ocultar la franja de audio, con la onda y el cursor (HU-106) |
| O | Pila en círculo: después de la última hoja viene la primera (flip, mesa de luz, panel); se guarda en la toma (HU-94) |
| C | Mostrar u ocultar la carta de timing: rectángulo tenue de 35 × 22,5 mm arriba a la izquierda de las keys, para dibujar el chart a mano (HU-105) |
| G | Mostrar u ocultar la guía de campo: 12 campos en 16:9 a tamaño real, gris tinta bajo el grafito; gira con la hoja (HU-103) |
| F | Marcar la activa como hoja fija (o desmarcarla): debajo de todas, gris, fuera del flip y de la numeración; se sigue en la vecina (HU-89) |
| Ctrl+F | Dibujar en la hoja fija, con la hoja de la que se vino tenue de referencia; Ctrl+F, ← o → vuelven (HU-89) |
| Tab | Mostrar u ocultar el panel de hojas (queda guardado; HU-114) |
| 1 · 2 · 3 · 4 · 5 | Grafito (HB) · azul no-foto (construcción) · rojo (sombra 1) · verde (sombra 2 o highlight) · violeta (cierres); queda guardado (HU-99) |
| F5 | Selector de lápices, centrado sobre la hoja: el grafito y los cuatro colores con su muestra; F5 o Esc lo cierran (HU-100) |
| + / − | Zoom de a ~20 %, de 25 % a 400 %, hacia la punta del lápiz si está sobre la hoja (si no, hacia el centro); también los del teclado numérico. Se guarda en la toma; el panel y F3 lo muestran (HU-101) |
| Ctrl+1 | Zoom 100 %, centrada, sin cambiar el giro (HU-101) |
| Espacio + arrastrar con el lápiz | Pan: la hoja sigue a la punta y no se dibuja; el cursor pasa a la flecha de cuatro puntas mientras Espacio está apretado. El centro de la hoja no sale de la pantalla. Se guarda en la toma (HU-102) |
| Rueda, en su segundo modo (botón del centro) | Zoom continuo: un cuarto de vuelta duplica o reduce a la mitad (HU-101); el overlay de Wacom lo muestra como "Zoom" |
| Rueda, en su tercer modo | "Hojas": flipea como mantener la flecha, una hoja cada 30° (horario = la siguiente); al levantar el dedo queda activa (HU-95) |
| Botón lateral del lápiz | Menú del lápiz en la punta: los cinco lápices y mesa de luz, hojas, deshacer, rehacer y vista inicial (HU-116) |
| Z / Ctrl+Y | Deshacer / rehacer lo más reciente: el último trazo de la hoja activa o la última operación con las hojas (incluido mover) |

**Ejercicios de animación:** las teclas de la Mesa de animación (flip, mesa de luz, lápices, zoom y pan, deshacer, Inicio y Fin, Tab) salvo las de la toma (Insert, Supr, Ctrl+D, ↑ ↓, K, F, A, P, M, W, C, O, Ctrl+E, la guía de campo): la pila la arma el ejercicio y no se guarda. Más las de Ejercicios:

| Tecla | Qué hace |
| --- | --- |
| Enter | Ejercicio siguiente: otra semilla, la pila en blanco con su guía en cada hoja (→ es flip) (HU-121) |
| R | Repetir: la misma pila en blanco, con las mismas guías (HU-121) |
| G | Mostrar u ocultar las guías de todas las hojas, para comparar (HU-121). Con el ghost solo en las keys (F2), muestra el de todas las hojas y otra G vuelve a solo las keys (HU-125) |
| F4 | Menú de ejercicios: el prisma, el cilindro, el cono y la cabeza que giran (Volumen), los trazos en pila (Líneas, Curvas, Elipses, Perspectiva) y el modo mixto (HU-121, HU-122, HU-123, HU-130, HU-131) |
| F2 | Panel: los parámetros del ejercicio (las hojas y el ghost solo en las keys, entre ellos) y qué ejercicios entran en el modo mixto (HU-121, HU-125) |
