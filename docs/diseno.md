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
| `mesaLuzAnterior` | `#C8352E` | Mesa de luz (Mesa de animación): hojas anteriores, al 60 % la más cercana y al 30 % la segunda |
| `mesaLuzSiguiente` | `#2F8A4E` | Mesa de luz: hojas siguientes, ídem |

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
| Atajos (Ctrl+,) | Como la mesa "Atajos": título con el nombre de la app, los grupos en tres columnas (Lápiz · Hoja y papel, Vista · Ejercicios, Pantalla), teclas como chips y los gestos que no son teclas en gris claro. Sale del registro de atajos, así no se desactualiza | HU-78 |
| Mesa de luz | Como la mesa "Mesa de luz": las hojas anteriores teñidas de rojo y las siguientes de verde, más tenues cuanto más lejos; se apaga mientras se flipea | HU-91 |
| Panel de hojas (Tab) | Como la mesa "Panel de hojas": angosto (240 px) a la derecha, con la toma y si está guardada, el número grande en ámbar y el total, las miniaturas de la activa (marco ámbar) y de 3 antes y 3 después, el lápiz, la mesa de luz y el giro | HU-114 |
| Íconos | Como la mesa "Íconos": fondo gris oscuro redondeado; Ejercicios es la hoja con dos puntos azules y un trazo; Cartuchera, un lápiz ámbar; Mesa de animación, tres hojas apiladas con la pelota en grafito y, debajo, la del dibujo anterior en rojo; el instalador, la hoja y el lápiz juntos. Se generan en vector con `assets/icons/make-icons.ps1` | HU-70, HU-110 |

## Teclas (v1.3.0)

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
| 4 / 6 / 5 | Girar la vista de a 15° / volver a 0° | Igual (desde el 10 de octubre de 2026; las durezas se eligen con F5) |
| Shift + arrastrar con el lápiz | Girar la vista libremente, sin snap (HU-79) | Igual |
| Z / Ctrl+Y | — (sin deshacer) | Deshacer / rehacer |
| `[ ]` · `, .` · `- =` | Tamaño · blandura (fuerza en la goma) · techo, de la herramienta activa | Igual |
| Ctrl+S · Ctrl+N | Guardar el lápiz · hoja nueva | Igual |
| F3 · F9 · F10 · F12 | Latencia · calibrar el área útil · monitor siguiente · captura | Igual |
| Alt+F4 | Salir | Salir |

**Mesa de animación:** las teclas del lienzo, como Cartuchera, salvo A (afilar) y E (punta seca), que no están en el medio recortado (HU-112), Ctrl+[ ] (el papel de animación no tiene textura visual, HU-97), y F5, las de calibración ([ ] , . - =) y Ctrl+S (un solo grafito, la HB, HU-98); el costado (I) arranca apagado, más las de las hojas (HU-87) y el flip (HU-90). Las de las hojas se deshacen con Z.

| Tecla | Qué hace |
| --- | --- |
| ← / → | Flip: mantener la flecha muestra la hoja anterior o la siguiente; al soltarla, esa hoja queda activa. Mientras tanto el lápiz no dibuja y la mesa de luz no se ve |
| L | Mesa de luz: prende y apaga (queda guardada); 2 hojas atrás en rojo y 1 adelante en verde (mesa "Mesa de luz") |
| Insert | Hoja nueva en blanco después de la activa, que pasa a ser la activa |
| Ctrl+D | Duplicar la activa: la copia queda después y pasa a ser la activa |
| Ctrl+N | Limpiar la hoja activa |
| Supr | Borrar la hoja activa; si tiene trazos, un cartel pide Supr otra vez (mesa "Confirmación") |
| ↑ / ↓ | Mover la hoja activa un lugar antes o después en la pila (HU-88) |
| Inicio / Fin | Ir a la primera o a la última hoja, sin deshacer, como un flip (HU-115) |
| Tab | Mostrar u ocultar el panel de hojas (queda guardado; HU-114) |
| Z / Ctrl+Y | Deshacer / rehacer lo más reciente: el último trazo de la hoja activa o la última operación con las hojas (incluido mover) |
