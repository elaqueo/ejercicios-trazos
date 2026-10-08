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
| Lienzo | Hoja `#F5F0E6` en el área útil; el panel de ayuda (ejercicio, teclas, pincel, ángulo) va en la zona gris; si el área cubre toda la pantalla, se reduce a una línea en una esquina | HU-41, HU-10 |
| Guías | Punto a unir: anillo de 24 px con centro; punto de paso: anillo chico hueco; PF: rombo ámbar | HU-19, HU-21, HU-22, HU-28 |
| Puntero | **Cruz abierta** (opción A): cuatro trazos finos con el centro libre | HU-38 |
| Menú | Modo mixto destacado arriba; 15 ejercicios en 6 grupos; el actual en ámbar | HU-11 |
| Panel | Panel a la derecha, pestañas Ejercicio / Pincel / Área útil / Colores / Monitor | HU-12 |
| Pinceles | Panel a la derecha con filtro por carpeta, buscador y grilla de 3 columnas | HU-06 |
| Calibración | Pantalla oscura, indicador de pasos, marca ámbar en la esquina pedida | HU-10 |
