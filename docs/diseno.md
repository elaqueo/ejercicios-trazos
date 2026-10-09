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
| `tinta` | `#111111` | Trazo (siempre negro) |
| `guia` | `#2563EB` | Puntos a unir, líneas de muestra, ejes |
| `guia-suave` | `#9DB8F2` | Construcción: horizonte, líneas a PF (punteada) |
| `enfasis` | `#E08A1E` | Dirección, puntos de fuga, perfil de presión, selección |

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
| Menú (F4) | Modo mixto destacado arriba; 15 ejercicios en 7 grupos (Líneas, Curvas, Elipses, Perspectiva, Escritura, Memoria, Presión); el actual en ámbar; el cursor del teclado se frena en los extremos | HU-11, HU-20 |
| Panel (F2) | Panel a la derecha del área útil, pestañas Ejercicio (parámetros del actual y, en modo mixto, qué ejercicios participan) / Lápiz (mina activa y goma) / Pantalla (área útil y monitor). Los colores de la hoja quedaron fijos | HU-12, HU-20 |
| Lápices (F5) | Panel a la derecha con la imagen de la colección de grafito arriba y las diez minas con una muestra simulada; la activa resaltada. Reemplaza al selector de pinceles de libmypaint | HU-67 |
| Calibración | Pantalla oscura, indicador de pasos, marca ámbar en la esquina pedida | HU-10 |
| Íconos | Fondo gris oscuro redondeado: Ejercicios es la hoja con dos puntos azules y un trazo; Cartuchera, un lápiz ámbar; el instalador, los dos juntos. Se generan en vector con `assets/icons/make-icons.ps1` | HU-70 |

## Teclas (v1.0.0)

Todas se declaran en el registro único de atajos (HU-14); si dos acciones piden la misma tecla, la app avisa al iniciar.

| Tecla | Ejercicios | Cartuchera |
| --- | --- | --- |
| → / botón lateral del lápiz | Ejercicio siguiente | — |
| R | Repetir el ejercicio | — |
| I | Costado sí o no: apagado, la punta es siempre la vertical (HU-73) | Igual |
| E | Punta seca o mina: la punta seca hunde el papel sin grafito, para líneas blancas (HU-61) | Igual |
| A | Afilar la mina activa: la punta se gasta al dibujar, cada dureza por su cuenta (HU-62) | Igual |
| G | Mostrar u ocultar las guías (comparar) | — |
| F4 / F2 | Menú / panel | — |
| F5 | Selector de lápices | Selector de lápices |
| 1 a 0 | — (los números son de la vista) | Dureza 2H … 6B |
| 4 / 6 / 5, Shift + arrastrar | Girar la vista / volver a 0° | Shift + arrastrar |
| Z / Ctrl+Y | — (sin deshacer) | Deshacer / rehacer |
| `[ ]` · `, .` · `- =` | Tamaño · blandura (fuerza en la goma) · techo, de la herramienta activa | Igual |
| Ctrl+S · Ctrl+N | Guardar el lápiz · hoja nueva | Igual |
| F3 · F9 · F10 · F12 | Latencia · calibrar el área útil · monitor siguiente · captura | Igual |
| Alt+F4 | Salir | Salir |
