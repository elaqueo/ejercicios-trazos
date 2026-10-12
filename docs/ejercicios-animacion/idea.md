# Ejercicios de animación — La idea

12 de octubre de 2026 · Daniel

> Documento de idea. No hay requerimientos ni plan: esto es para seguir refinando qué queremos hacer y, sobre todo, qué no.

## Qué es

Una cuarta app de la suite, hija de dos: tiene la estructura de **Ejercicios de trazos** (una consigna generada, se dibuja, se pasa a la siguiente) y la mesa de **Mesa de animación** (una pila de hojas, la mesa de luz, el flip a mano). Ejercicios entrena el trazo suelto; acá se entrena lo que pide la animación: que el mismo trazo, la misma forma o el mismo volumen se mantengan **de una hoja a la otra**, o cambien de a poco y de manera creíble.

La práctica es la de siempre en papel: dibujar en varias hojas, ponerlas sobre la mesa de luz y flipearlas para ver si algo salta. La app pone la consigna en cada hoja; la mano hace el resto.

Corre sobre lo mismo que la Mesa: `libs/lienzo` con la pila de hojas (`sheets = true`), el perfil de medio de animación (sin desgaste ni punta seca), el papel de animación, el grafito HB y los lápices de color. Practicar con la herramienta con la que después se anima es justamente la idea que había quedado anotada en la Mesa ("Ejercicios para animación", 10 de octubre de 2026); esta app la absorbe y le suma ejercicios propios.

El nombre es **Ejercicios de animación** (ejecutable `ejerciciosanimacion`), paralelo a *Ejercicios de trazos*: es el mismo tipo de app para otra disciplina.

## Qué no es

| Fuera | Por qué |
| --- | --- |
| Animar una toma | Eso es la Mesa. Acá la pila es la de un ejercicio: se arma sola y se descarta al pasar al siguiente. |
| Guardar, tomas, exportar | Como en Ejercicios: un ejercicio es una práctica, no un trabajo. Si sale algo que valga la pena, se dibuja en la Mesa. |
| Evaluar o puntuar | Igual que Ejercicios: está fuera de alcance (`docs/alcance-v1.md`). El que juzga es el ojo al flipear. |
| Audio, carta de timing, hoja fija | Son herramientas de animar una toma, no de practicar. Se suman solo si un ejercicio las pide. |
| Inbetweening automático | La consigna puede dibujar una guía; el intermedio lo dibuja la mano. |

## El modelo: un ejercicio es una pila con una consigna por hoja

- Un ejercicio arma **una pila de N hojas** y pone en cada una **su propia guía**: la misma en todas (repetir) o una que cambia de hoja en hoja (girar, mover). Las guías van debajo del grafito, tenues, como en Ejercicios.
- Como en Ejercicios, cada ejercicio es **determinista por semilla**: "repetir" vuelve a armar exactamente la misma pila, en blanco, y "siguiente" arma otra.
- En la pila todo es como en la Mesa: **← →** flipean (mantener la flecha muestra la hoja vecina; al soltar, queda activa), **L** prende la mesa de luz, **G** oculta y muestra las guías para comparar lo dibujado contra la consigna.
- **La guía sigue a su hoja**: al flipear, cada hoja se ve con la suya, y la mesa de luz muestra lo dibujado en las vecinas (no sus guías, que serían ruido).
- Hay deshacer (Z), a diferencia de Ejercicios: en una pila de doce hojas, perder la hoja entera por un trazo errado es castigar de más (decidido con el usuario el 12 de octubre de 2026).
- El menú (F4) y el modo mixto son los de Ejercicios; el panel (F2) tiene los parámetros de cada ejercicio, y además cuántas hojas.

**Choque de teclas a resolver.** En Ejercicios, → pasa al ejercicio siguiente; en la Mesa, ← → flipean. Acá manda el flip, porque es el corazón de la práctica: "siguiente" tiene que ir a otra tecla (Enter, o el botón del lápiz como hoy en Ejercicios). Queda abierto.

## Ejercicio 1: trazos de Ejercicios en pila

Los ejercicios que ya existen (rectas, hatching, curvas, elipses, cajas, cajas rotadas…) con **la misma consigna en todas las hojas**. Se dibuja hoja por hoja y después se flipea, o se mira con la mesa de luz: si la elipse cambia de grado, si la recta tiembla distinto, si la caja se deforma, salta enseguida. Es la consistencia que pide repetir un personaje en cada dibujo.

No se reescriben: los generadores de `apps/ejercicios/src/exercises` se comparten entre las dos apps. Por eso habría que sacarlos a una biblioteca (ver más abajo). Cuáles tienen sentido en pila se decide con la app andando; escritura y presión, probablemente no.

## Ejercicio 2: el prisma que gira

Un **prisma rectangular** (largo, ancho y alto al azar, como las cajas de "Cajas rotadas") que **gira sobre uno de sus ejes**, de a un paso fijo por hoja: en la hoja 1 está a 0°, en la 2 a 15°, en la 3 a 30°, y así.

- En cada hoja va el prisma **con el giro de esa hoja**, en wireframe y tenue: **el ghost**. Se dibuja encima, a mano, siguiendo el volumen.
- **G** oculta el ghost para ver solo lo dibujado; **L** muestra las hojas vecinas por transparencia; **← →** flipean, y ahí se ve si el volumen se mantiene o si "respira".
- **Parámetros:** el eje (vertical, horizontal, de profundidad, o al azar), el paso (5° a 45°), cuántas hojas (lo que dé una vuelta o un tramo), la perspectiva (de casi isométrica a fuerte), si se ven las aristas ocultas (punteadas) y el tamaño.
- **Por qué un prisma:** es la forma más simple que tiene volumen, caras y aristas que cambian al girar; es la base de girar una cabeza o un torso. Lo difícil no es dibujar una caja: es dibujar la misma caja, girada un poco, doce veces seguidas.
- **Variantes para más adelante:** ghost solo en algunas hojas (la primera, la del medio y la última, como keys) y las intermedias de memoria; un ghost que se apaga a los pocos segundos, como en Ghosting; otras formas que giran (cilindro, cono, una cabeza simplificada).

## La mesa de luz y el flip

Los de la Mesa, sin cambios: 2 hojas hacia atrás en naranja y 1 hacia adelante en turquesa, que se apagan mientras se flipea y vuelven al soltar. Ctrl+L (solo keys) no tiene sentido sin roles; no va.

## Atajos (provisorios)

Todo lo que no se nombra es como en la Mesa (zoom, pan, giro de la vista, F3, F9, F10, Ctrl+, Ctrl+Q).

| Tecla | Qué hace |
| --- | --- |
| `←` `→` | Flip: hoja anterior / siguiente |
| `Inicio` `Fin` | Primera / última hoja |
| `L` | Mesa de luz |
| `G` | Ocultar / mostrar las guías (el ghost) |
| `R` | Repetir el ejercicio (la misma pila, en blanco) |
| (a definir) | Ejercicio siguiente (Enter, o el botón del lápiz) |
| `F4` / `F2` | Menú de ejercicios / parámetros |
| `F5`, `1` a `5` | Lápices, como en la Mesa |
| `Z` / `Ctrl+Y` | Deshacer / rehacer en la hoja |
| `+` `−` | Zoom |

## Ideas abiertas

- ¿Qué tecla es "siguiente"? ¿Y el botón del lápiz: siguiente, como en Ejercicios, o el menú del lápiz, como en la Mesa?
- ~~¿Deshacer sí o no?~~ Sí (12 de octubre de 2026).
- ~~¿Se guarda lo dibujado?~~ No, como en Ejercicios; a lo sumo F12 para una captura (12 de octubre de 2026).
- ¿El ghost se puede pedir solo en algunas hojas desde el primer día, o es una variante posterior?
- Otros ejercicios de animación, para después: **intercalar** (dos keys dadas como guía y el intermedio a mano, con su chart: mitades, tercios, ease), **arcos y spacing** (una trayectoria con marcas; la pelota en cada posición), **la pelota que rebota** (squash y stretch), **el cilindro que gira**.

## Qué se reutiliza y qué habría que hacer

Solo a nivel idea, para medir el tamaño.

**Se reutiliza tal cual:** `libs/lienzo` (la pila de hojas, el flip, la mesa de luz, los lápices de color, el zoom y el pan, el panel de hojas), `libs/drymedia` con el perfil de animación, `libs/appkit` (guías, atajos, configuración, menú, panel) y la estructura de Ejercicios: `Exercise`, `ExerciseSession` y `ExerciseMenu`.

**Es nuevo:**

- **Una guía por hoja.** Hoy la guía es una sola para todo el lienzo (`Lienzo::setGuides`): se vería en todas las hojas y al flipear. Hace falta que cada hoja tenga la suya (en el render, por id de hoja, como su textura) y que la mesa de luz no muestre las guías de las vecinas.
- **Los ejercicios en una biblioteca** (`libs/exercises`): sacar los generadores y su sesión de `apps/ejercicios` para que los usen las dos apps, sin duplicar.
- **El generador del prisma que gira:** proyectar un prisma 3D girado a 2D con perspectiva, por hoja, con aristas ocultas.
- **Una sesión de pila:** armar las N hojas del ejercicio con sus guías, vaciarlas al repetir y descartarlas al pasar al siguiente.
- **La app** (`apps/ejerciciosanimacion`) y su lugar en el instalador.
