# Ejercicios de animación — Requerimientos y backlog

12 de octubre de 2026 · Daniel

Backlog de la app definida en [idea.md](idea.md): la cuarta app de la suite, Ejercicios de trazos sobre la pila de hojas de Mesa de animación. Mismo enfoque ágil que [requerimientos-backlog.md](../requerimientos-backlog.md) (roles, sprints, MoSCoW, puntos Fibonacci, Definition of Ready y Done) y mismo molde que los [requerimientos de Mesa de animación](../mesa-de-animacion/requerimientos.md). La numeración sigue la de la suite: HU-118 en adelante.

> **Estado (12 de octubre de 2026):** todas las historias están en los [issues](https://github.com/elaqueo/ejercicios-trazos/issues?q=label%3A%22E12+%C2%B7+Ejercicios+de+animaci%C3%B3n%22) (#118 a #131, la issue #N es la HU-N) y el [tablero](https://github.com/users/elaqueo/projects/3), con el milestone de su fase. El estado vivo está ahí; este documento es el plan.

## Visión y MVP

Para Daniel, que practica el trazo con Ejercicios y anima con la Mesa: practicar lo que pide la animación (que la misma forma se mantenga de una hoja a la otra, o cambie de a poco) con la consigna puesta en cada hoja, la mesa de luz y el flip a mano.

1. **Objetivo de producto:** ejercicios en pila (la misma consigna en varias hojas, o una que cambia de hoja en hoja) para entrenar consistencia y volumen en movimiento, con la herramienta con la que se anima.
2. **Objetivo del MVP (fin de la Fase 1):** abrir la app, elegir con F4 "Prisma que gira" o un ejercicio de Ejercicios en pila, dibujar hoja por hoja sobre el ghost de cada una, flipear con ← → y ver las vecinas con L; Enter pasa a otro ejercicio y R repite el mismo en blanco.
3. **Objetivo de plataforma:** que los ejercicios y la guía por hoja vivan en bibliotecas, para que la app sea chica: Ejercicios y la Mesa no se duplican, se combinan.

Decisiones firmes que acotan el backlog (ver idea.md, "Qué no es"): no anima tomas, no guarda ni exporta, no evalúa, sin audio, carta de timing ni hoja fija, sin inbetweening automático.

## Requerimientos no funcionales

| ID | Requerimiento | Criterio de verificación |
| --- | --- | --- |
| RNF-01 | Flip como en la Mesa | El flip y la mesa de luz cumplen los RNF-01 y RNF-03 de la Mesa (menos de 18 ms de la tecla al Present; tinte en menos de 50 ms al soltar), también con las guías por hoja; queda en el log |
| RNF-02 | Dibujar como en la Mesa | La latencia del trazo es la de la Mesa (mediana ≈ 12 ms en F3); las guías no suman trabajo al hilo de simulación |
| RNF-03 | Determinista | La misma semilla y los mismos parámetros arman la misma pila con las mismas guías (como Ejercicios, HU-15); R lo usa |
| RNF-04 | Armar la pila rápido | Pasar a otro ejercicio (Enter) o repetirlo (R) con 12 hojas tarda menos de 300 ms hasta que se puede dibujar |
| RNF-05 | Un solo lienzo, unos solos ejercicios | `apps/ejerciciosanimacion` no reimplementa nada de `libs/lienzo` ni de los ejercicios: lo que necesita se agrega en las bibliotecas y lo pueden usar las otras apps |
| RNF-06 | Arranque directo | La app abre en el último ejercicio con sus parámetros (no lo dibujado: no se guarda) |

## Épicas

Cuatro épicas, 7 historias en el MVP y 6 para después.

| ID | Épica | Objetivo | Historias | Fase |
| --- | --- | --- | --- | --- |
| E1 | Plataforma | Ejercicios en una biblioteca, guía por hoja en el lienzo, la app | HU-118 a HU-120 | 1 |
| E2 | Sesión de pila | Armar la pila del ejercicio, repetir, siguiente, menú y parámetros | HU-121 | 1 |
| E3 | Ejercicios | Trazos de Ejercicios en pila, prisma que gira, variantes y ejercicios nuevos | HU-122, HU-123, HU-125 a HU-131 | 1–2 |
| E4 | Entrega | Instalador, docs, prueba de la app real | HU-124 | 1 |

## Historias

Cada historia indica tipo, prioridad MoSCoW y estimación; debajo, sus criterios de aceptación. Las teclas son las de idea.md.

### E1 · Plataforma

118. **HU-118 · Ejercicios en una biblioteca** (enabler · Must · 2 pts). Como desarrollador, quiero que los ejercicios, su sesión y su menú sean una biblioteca, para que los usen Ejercicios de trazos y Ejercicios de animación sin duplicarlos.
    1. Lo que hoy es `ejercicios_core` en `apps/ejercicios` (`Exercise`, los 16 ejercicios, `ExerciseSession`, `ExerciseMenu`) pasa a `libs/exercises`, con sus pruebas.
    2. Ejercicios de trazos no cambia: mismos ejercicios, mismas guías (las pruebas de cada ejercicio siguen pasando sin tocarlas) y la prueba de la app real igual.
119. **HU-119 · Guía por hoja** (enabler · Must · 5 pts). Como desarrollador, quiero que cada hoja de la pila pueda tener su propia guía, para que el ghost de cada hoja se vea solo en ella.
    1. `Lienzo` acepta una guía (QPicture, en coordenadas de la hoja) por hoja, por id; se ve debajo del grafito, como la de Ejercicios.
    2. Al flipear, cada hoja se ve con su guía; la mesa de luz muestra el grafito de las vecinas, no sus guías.
    3. La guía no queda en la hoja: va en el render, como la guía de campo (HU-103), y una guía que se oculta (G) no repinta la hoja.
    4. La guía única de hoy (`setGuides`) sigue igual para Ejercicios de trazos y Cartuchera (prueba de regresión).
    5. El flip sigue en RNF-01, medido con 12 hojas con guía.
120. **HU-120 · App Ejercicios de animación** (enabler · Must · 2 pts). Como usuario, quiero una app `ejerciciosanimacion` sobre el lienzo de la Mesa, para empezar a practicar en pila.
    1. `apps/ejerciciosanimacion` arranca como la Mesa (monitor, área útil, F3, F9, F10, Ctrl+, Ctrl+Q), con el nombre "Ejercicios de animación", el perfil de animación, el papel de animación, el grafito HB, los lápices de color (1 a 5, F5), el zoom y el pan.
    2. ← → flipean, Inicio y Fin saltan a la primera y la última hoja, L prende la mesa de luz (naranja y turquesa, como la Mesa) y Tab el panel de hojas; Z y Ctrl+Y deshacen y rehacen en la hoja.
    3. El botón del lápiz abre el menú alrededor de la punta, como en la Mesa.
    4. No hay toma: la pila vive en memoria y no se guarda (sin `useTake`).
    5. Comparte config con la familia (monitor y área útil) y tiene la suya (sección `ejerciciosanimacion`).

### E2 · Sesión de pila

121. **HU-121 · Sesión de pila** (Must · 5 pts). Como practicante, quiero que cada ejercicio arme su pila con su guía en cada hoja, para dibujar sin preparar nada.
    1. Un ejercicio de pila genera, a partir de sus parámetros y una semilla, N guías (una por hoja) y su geometría ideal por hoja; la sesión arma una pila de N hojas en blanco con esas guías.
    2. **Enter** pasa al ejercicio siguiente (otra semilla, o otro ejercicio en modo mixto) y descarta la pila anterior; **R** repite: la misma pila, en blanco, en la primera hoja. **G** oculta y muestra las guías de todas las hojas.
    3. **F4** abre el menú de ejercicios (con modo mixto, como Ejercicios) y **F2** el panel con los parámetros del ejercicio, incluida la cantidad de hojas (de 2 a 24).
    4. El ejercicio elegido y sus parámetros quedan en config (RNF-06).
    5. Cumple RNF-03 y RNF-04 (medido con 12 hojas).

### E3 · Ejercicios

122. **HU-122 · Trazos de Ejercicios en pila** (Must · 3 pts). Como practicante, quiero los ejercicios de Ejercicios con la misma consigna en varias hojas, para ver al flipear si el trazo se mantiene.
    1. Los ejercicios de líneas, curvas, elipses y perspectiva se ofrecen en el menú, cada uno con la misma guía (misma semilla) en todas las hojas; escritura, ghosting y presión no (no tienen sentido en pila).
    2. Sus parámetros son los de Ejercicios más la cantidad de hojas (6 por defecto).
    3. Prueba unitaria: todas las hojas reciben la misma guía y repetir da las mismas.
123. **HU-123 · Prisma que gira** (Must · 8 pts). Como practicante, quiero un prisma rectangular que gira hoja por hoja con su ghost en cada una, para practicar mantener el volumen al girar.
    1. Un prisma de largo, ancho y alto al azar (por semilla) gira sobre uno de sus ejes, de a un paso por hoja; en cada hoja va su wireframe con el giro de esa hoja, tenue (el ghost).
    2. Parámetros: el eje (vertical, horizontal, de profundidad o al azar), el paso (5° a 45°), la cantidad de hojas, la perspectiva (de casi isométrica a fuerte), las aristas ocultas (punteadas o no) y el tamaño.
    3. Diseño primero en el canvas (mesa "Prisma que gira"): color y opacidad del ghost, cómo se ven las aristas ocultas y si la hoja muestra el ángulo.
    4. Pruebas unitarias: la proyección de las esquinas para giros conocidos, que cada hoja suma un paso, que las aristas ocultas son las de atrás y que la misma semilla da el mismo prisma.

### E4 · Entrega

124. **HU-124 · Instalador, docs y prueba de la app real** (Must · 2 pts). Como usuario, quiero Ejercicios de animación en el instalador de Trazos y documentada, para usarla en la laptop.
    1. `package.ps1` y `trazos.iss` incluyen `ejerciciosanimacion.exe` con su acceso en el menú Inicio.
    2. README, `docs/diseno.md` (teclas y mesas) y una `arquitectura.md` de la app (la guía por hoja, la sesión de pila) al día.
    3. La prueba de la app real (`smoke.ps1`) abre la app y prueba F4, Enter, R, G, el flip y la mesa de luz.

### Para después (Fase 2)

Las variantes y ejercicios nuevos que salieron de la idea. Se estiman y se refinan cuando la Fase 1 esté en uso.

125. **HU-125 · Ghost solo en algunas hojas** (Could · 3 pts). Como practicante, quiero el ghost solo en la primera, la del medio y la última hoja (como keys), para dibujar las intermedias sin guía.
126. **HU-126 · Ghost que se apaga** (Could · 2 pts). Como practicante, quiero que el ghost de cada hoja se vea unos segundos y desaparezca, como en Ghosting, para dibujar de memoria.
127. **HU-127 · Intercalar entre dos keys** (Could · 5 pts). Como practicante, quiero dos keys dadas como guía y un chart de spacing (mitades, tercios, ease), para dibujar los intermedios a mano.
128. **HU-128 · Arcos y spacing** (Could · 5 pts). Como practicante, quiero una trayectoria en arco con marcas de spacing, para dibujar el objeto en cada posición, hoja por hoja.
129. **HU-129 · La pelota que rebota** (Could · 5 pts). Como practicante, quiero la consigna de la pelota que rebota (posiciones, squash y stretch), para el ejercicio clásico de animación.
130. **HU-130 · Cilindro que gira** (Could · 5 pts). Como practicante, quiero un cilindro que gira hoja por hoja con su ghost, como el prisma, para practicar volúmenes curvos (pedido del 12 oct 2026).
    1. Un cilindro de radio y largo al azar (por semilla, de 1:3 a 3:1) gira sobre un eje, un paso por hoja, con el ghost del prisma (gris tinta, ocultas punteadas, el ángulo en la esquina).
    2. Una costura (una generatriz y una marca en la tapa) deja ver el giro, aun sobre su propio eje; un parámetro la saca. Por defecto gira sobre el eje horizontal (da vueltas de punta).
    3. La silueta son las dos generatrices de canto a la cámara; las tapas se proyectan exactas (muestreadas) y lo que tapan el costado o la tapa de frente va punteado.
    4. La cámara, la perspectiva y el estilo del ghost son los del prisma, en común; el prisma no cambia.
    5. Diseño primero en el canvas (mesa "Cilindro que gira"). Pruebas: la elipse de la tapa, la silueta tangente a las tapas, lo oculto, un paso por hoja, la semilla y que entre en la hoja.
131. **HU-131 · Cono y cabeza que giran** (Could · 5 pts). Como practicante, quiero un cono y una cabeza simplificada que giran como el prisma y el cilindro, para practicar más volúmenes curvos (salió de dividir la HU-130).
    1. Cono: radio y altura al azar (de 1:3 a 3:1), la base en elipse exacta, la silueta del vértice tangente a la base (ninguna visto de punta), lo oculto punteado y la costura; por defecto sobre el eje horizontal.
    2. Cabeza: Loomis simplificada, solo la estructura (la esfera del cráneo, la línea media y la de las cejas, los cortes laterales, la línea de la cara al mentón y la mandíbula); la línea media hace de costura; por defecto sobre el eje vertical (de lado a lado).
    3. Diseño primero en el canvas (mesa "Cono y cabeza que giran"). Pruebas: la silueta del cono, lo oculto, el contorno de la esfera, la cabeza de frente y de costado, un paso por hoja, la semilla y que entren en la hoja.

## Plan por fases

1. **Fase 1 — MVP: practicar en pila** (HU-118 a HU-124; 27 pts)
    1. Ejercicios en una biblioteca, guía por hoja, la app, la sesión de pila, los trazos de Ejercicios en pila, el prisma que gira, instalador.
    2. Salida: un prisma girando 12 hojas de a 15°, dibujado sobre su ghost y flipeado.
    3. *Cerrada (12 oct 2026) y entregada en la v1.5.0.*
2. **Fase 2 — Más ejercicios** (HU-125 a HU-131; 30 pts)
    1. Variantes del ghost e intercalar, arcos, pelota y formas que giran, según lo que pida el uso.

Las fases son milestones propios de la app ("Ejercicios de animación · Fase N"). El orden dentro de la Fase 1 es el numérico: primero la plataforma (HU-118 a HU-120), después la sesión y los ejercicios.

## Diseño

Cada pantalla nueva tiene su mesa en el canvas de diseño de Trazos antes de implementarse: el ghost del prisma (mesa "Prisma que gira"). El menú, el panel y la página de atajos son los de Ejercicios y la Mesa; si hace falta un token nuevo (el color del ghost), va a `Theme.h`.

## Decisiones abiertas

Cada una se cierra en la historia que la toca.

1. ~~Cuántas hojas por defecto en el prisma~~: 12, media vuelta a 15° (HU-123).
2. ~~Si la hoja muestra el ángulo del prisma~~: sí, chico abajo a la derecha, en el gris del número de la hoja (HU-123, mesa "Prisma que gira"). El ghost va en gris tinta al 50 % (`kGhostOpacidad`) y las aristas ocultas punteadas y más finas.
3. Qué ejercicios de Ejercicios entran en pila, más allá de la primera selección (HU-122).
