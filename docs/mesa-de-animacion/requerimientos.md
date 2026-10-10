# Mesa de animación — Requerimientos y backlog

10 de octubre de 2026 · Daniel

Backlog de la app definida en [idea.md](idea.md): la tercera app de la suite, para la etapa de rough animation. Mismo enfoque ágil que [requerimientos-backlog.md](../requerimientos-backlog.md) (roles, sprints, MoSCoW, puntos Fibonacci, Definition of Ready y Done) y mismo molde que el plan por fases de Cartuchera.

> **Estado:** documento de planificación. Las historias se cargan al tablero por fase, cuando esa fase arranca; hasta entonces viven acá.

## Visión y MVP

Para Daniel, que anima en papel y quiere la mesa de luz sin papel: una pila de hojas sobre una mesa de luz, con el flip controlado por la mano, sobre el mismo grafito de Cartuchera.

1. **Objetivo de producto:** bocetar keys y sentir el timing y el spacing flipeando, con mesa de luz, lápices con significado, audio de referencia y exportación a PNG.
2. **Objetivo del MVP (fin de la Fase 1):** abrir la app en la última toma, dibujar con el grafito, agregar hojas con Insert, reordenarlas, flipear con ← → sin ninguna demora y ver las hojas vecinas en rojo y verde al soltar. Todo guardado solo.
3. **Objetivo de plataforma:** que la pila de hojas y el tinte entren en `libs/lienzo` sin duplicar el lienzo, para que Mesa de animación sea una app chica sobre las mismas bibliotecas que Cartuchera y Ejercicios.

Decisiones firmes que acotan el backlog (ver idea.md, "Qué no es"): sin play ni fps ni hold por hoja, sin layers, sin cámara, sin color plano, sin vectores, sin inbetweening, una toma por archivo.

## Requerimientos no funcionales

El flip es el producto; lo demás son restricciones heredadas de Cartuchera.

| ID | Requerimiento | Criterio de verificación |
| --- | --- | --- |
| RNF-01 | Flip instantáneo | Al apretar ← o →, la hoja nueva está en pantalla en el frame siguiente: menos de 17 ms a 60 Hz desde la tecla hasta la presentación, medido con las estadísticas de F3; también manteniendo la tecla apretada (autorepetición) y con el anillo |
| RNF-02 | Pila en RAM | Una toma de 120 hojas (5 segundos en unos a 24 fps) abierta entera en memoria, con menos de 1,5 GB en la desktop (16 GB) y sin que el flip se degrade con el tamaño de la pila |
| RNF-03 | Mesa de luz sin demora perceptible | Al soltar la tecla, las hojas teñidas aparecen en menos de 50 ms; mientras se flipea no se compone nada |
| RNF-04 | Dibujar como en Cartuchera | La latencia del trazo sobre la hoja activa es la de Cartuchera (mediana ≈ 12 ms en F3); la pila no suma trabajo al hilo de simulación |
| RNF-05 | Guardado invisible | El guardado automático nunca frena el trazo ni el flip: se hace fuera de los hilos de simulación y render, por hoja cambiada |
| RNF-06 | Arranque directo | La app abre en la última toma, en la última hoja activa, con la vista como quedó |
| RNF-07 | Un solo lienzo | `apps/mesadeanimacion` no reimplementa nada de `libs/lienzo`; lo que la pila necesita se agrega en `lienzo` o `drymedia` y lo pueden usar las otras apps |
| RNF-08 | Formato abierto | Una toma es una carpeta con archivos que se pueden leer sin la app (JSON legible y hojas en un formato documentado) |

## Épicas

Nueve épicas, 30 historias. Las dos primeras son plataforma; el MVP es la Fase 1.

| ID | Épica | Objetivo | Historias | Fase |
| --- | --- | --- | --- | --- |
| E1 | Pila en el lienzo | Varias hojas en `lienzo`, una activa en simulación y las demás listas para mostrar; tinte en el render | HU-81 a HU-84 | 0–1 |
| E2 | Toma y hojas | Formato de toma, guardado automático, agregar, duplicar, borrar, reordenar, hoja fija | HU-85 a HU-89 | 1–2 |
| E3 | Flip y mesa de luz | ← →, mesa de luz rojo/verde, cuántas hojas, solo keys, círculo, anillo, roll | HU-90 a HU-96 | 1–2 |
| E4 | Lápices y papel | Papel de animación, grafito único, lápices de color, F5 | HU-97 a HU-100 | 1–2 |
| E5 | Vista | Zoom, pan, guía de campo | HU-101 a HU-103 | 2 |
| E6 | Rol y chart | Key / breakdown, número en la esquina, carta de timing | HU-104 a HU-105 | 2 |
| E7 | Audio | Onda, play/pausa, marcas | HU-106 a HU-108 | 3 |
| E8 | Exportación | Secuencia PNG | HU-109 | 3 |
| E9 | Entrega | App en el instalador, docs, prueba de la app real | HU-110 | 1 |

## Historias

Cada historia indica tipo, prioridad MoSCoW y estimación; debajo, sus criterios de aceptación. Las teclas son las provisorias de idea.md y se confirman en la historia que las usa.

### E1 · Pila en el lienzo

81. **HU-81 · Spike: flip con hojas reales** (spike · Must · 3 pts). Como desarrollador, quiero medir cuánto tarda cambiar la hoja que se muestra y la hoja que se simula, para decidir la arquitectura de la pila antes de construirla.
    1. Con 120 hojas de drymedia dibujadas (tiles tocados en todas), cambiar la hoja presentada tarda menos de un frame y cambiar la hoja activa de la simulación menos de 5 ms, medido en la desktop.
    2. Se mide la memoria por hoja en las dos representaciones (papel con tiles y la imagen lista para mostrar) y se decide cuál se guarda para las hojas no activas; si la imagen completa no entra en RNF-02, se decide una forma más chica (tono por celda, tiles).
    3. Salida: una nota en `docs/mesa-de-animacion/spikes/` con las mediciones y la decisión; el código del spike es descartable.
82. **HU-82 · Pila de hojas en el lienzo** (enabler · Must · 5 pts). Como desarrollador, quiero que `lienzo` tenga una pila de hojas con una activa, para que el trazo caiga en la hoja de arriba y las demás existan sin costo en el hilo de simulación.
    1. `lienzo::Lienzo` expone una pila: agregar, quitar, mover y elegir la hoja activa; Cartuchera y Ejercicios siguen con una sola hoja sin cambiar su código.
    2. Cambiar la hoja activa cambia el papel que simula el hilo de simulación sin perder trazos ni deshacer de la hoja anterior (deshacer es por hoja).
    3. Cada hoja no activa guarda lo que decidió HU-81 y su imagen se presenta sin recalcular.
    4. Pruebas unitarias: la hoja activa recibe el trazo, las otras no; mover una hoja conserva su contenido.
83. **HU-83 · Tinte de la mesa de luz en el render** (enabler · Must · 5 pts). Como desarrollador, quiero que el render componga la hoja activa sobre hojas teñidas, para que la mesa de luz sea un modo del render y no una copia de píxeles.
    1. El render recibe hasta N hojas anteriores y M siguientes con un color y una opacidad por hoja y las compone bajo la hoja activa, en orden.
    2. Con N = M = 0 el render es el de hoy, byte a byte (prueba de regresión con hash).
    3. Componer N + M = 6 hojas de tamaño tableta tarda menos de 8 ms en la desktop (GPU o CPU, lo que dé el spike).
84. **HU-84 · App Mesa de animación** (enabler · Must · 2 pts). Como usuario, quiero una app `mesadeanimacion` que abra sobre el lienzo, para empezar a usar la pila.
    1. `apps/mesadeanimacion` arranca como Cartuchera (monitor, área útil, F3, F9, F10, Ctrl+, con sus atajos) y con el nombre "Mesa de animación".
    2. Comparte config con la familia (monitor y área útil) y tiene la suya para la toma y la vista.

### E2 · Toma y hojas

85. **HU-85 · Formato de toma** (enabler · Must · 5 pts). Como usuario, quiero que una toma sea una carpeta que puedo copiar y abrir en la otra máquina, para no depender de la app para ver qué hay.
    1. Una toma es una carpeta con `toma.json` (versión, orden de hojas, hoja activa, roles, marcas de audio, ruta del audio, vista) y un archivo por hoja con el estado de la simulación (formato propio documentado en el `arquitectura.md` de la app, sin pérdida).
    2. Abrir la toma reconstruye la pila idéntica: hash de cada hoja igual al guardado (prueba unitaria con una toma de muestra).
    3. Un archivo corrupto o faltante no impide abrir la toma: la hoja queda en blanco y se avisa en el log y en F3.
86. **HU-86 · Guardado automático** (Must · 3 pts). Como animador, quiero que la toma se guarde sola, para no pensar en archivos mientras flipeo y dibujo.
    1. Cada hoja cambiada se guarda al soltar el lápiz o al cambiar de hoja, en un hilo aparte; `toma.json` al cambiar el orden, el rol o la vista.
    2. Cerrar la app con Alt+F4 espera a que termine lo pendiente (menos de 2 s con 120 hojas).
    3. No hay Ctrl+S; el atajo queda libre y la lista de atajos no lo muestra.
    4. Al arrancar se abre la última toma (RNF-06); sin ninguna, se crea "toma-1" en la carpeta de datos.
87. **HU-87 · Agregar, duplicar, borrar y limpiar hojas** (Must · 3 pts). Como animador, quiero manejar hojas con una tecla, para que "acá falta un dibujo" no corte el ritmo.
    1. Insert agrega una hoja en blanco justo después de la activa y la deja activa.
    2. Un atajo duplica la activa (misma hoja, después) y otro la limpia; borrar pide confirmación solo si la hoja tiene trazo (una tecla más).
    3. Las tres se pueden deshacer con Z, como un trazo.
    4. La hoja nueva tiene el papel y el tamaño de la toma.
88. **HU-88 · Reordenar con ↑ ↓** (Must · 2 pts). Como animador, quiero mover la hoja activa un lugar con ↑ y ↓, para corregir el orden sin menú.
    1. ↑ la acerca al principio y ↓ al final; en los extremos se queda.
    2. El número de la esquina se actualiza en todas las hojas afectadas.
    3. Se puede deshacer.
89. **HU-89 · Hoja fija** (Should · 3 pts). Como animador, quiero una hoja debajo de todas que no participe del flip, para tener el layout o la model sheet siempre a la vista.
    1. Un atajo marca la activa como fija: pasa al fondo, sin tinte, visible con la mesa de luz prendida o apagada.
    2. ← → la saltean; ↑ ↓ no la mueven; solo puede haber una (marcar otra desmarca la anterior).
    3. Se puede dibujar en ella eligiéndola con un atajo propio (no con el flip).

### E3 · Flip y mesa de luz

90. **HU-90 · Flip con ← →** (Must · 3 pts). Como animador, quiero pasar de hoja con ← y → al instante, para probar el timing con la mano.
    1. Cumple RNF-01, también con la tecla mantenida (autorepetición de Windows) y con cualquier tamaño de pila.
    2. En los extremos se queda (salvo HU-94).
    3. Mientras hay una tecla de flip apretada, la mesa de luz no se muestra (HU-91); al soltar, vuelve en menos de 50 ms.
    4. Prueba de la app real (smoke): 30 flips seguidos quedan en el log con su tiempo y ninguno pasa de un frame.
91. **HU-91 · Mesa de luz** (Must · 5 pts). Como animador, quiero ver las hojas anteriores en rojo y las siguientes en verde, con desvanecimiento, para dibujar en relación con las vecinas.
    1. Un atajo la prende y apaga; se guarda en config.
    2. Por defecto 2 hojas hacia atrás en rojo y 1 hacia adelante en verde; la más lejana más tenue.
    3. La hoja activa siempre se ve sin tinte y encima de todo.
    4. Los colores exactos y las opacidades son tokens del tema (`Theme.h`) y figuran en la mesa de diseño de la app.
92. **HU-92 · Cuántas hojas** (Should · 2 pts). Como animador, quiero ajustar cuántas hojas se ven hacia atrás y hacia adelante, para ver más contexto en una pose y menos en un detalle.
    1. Atajos para subir y bajar cada lado, de 0 a 4; el valor se ve un momento en la hoja y queda en config.
93. **HU-93 · Solo keys** (Should · 2 pts). Como animador, quiero que la mesa de luz muestre solo las hojas marcadas como key, para dibujar un breakdown entre dos claves sin el ruido del resto.
    1. Un atajo alterna el modo; con él, los conteos de HU-92 cuentan keys, no hojas.
    2. Depende de HU-104.
94. **HU-94 · Pila en círculo** (Should · 1 pt). Como animador, quiero que después de la última hoja venga la primera, para flipear un ciclo de caminata y probar el empalme.
    1. Un atajo lo prende y apaga; se guarda en la toma.
    2. Con el círculo, la mesa de luz también da la vuelta.
95. **HU-95 · El anillo pasa hojas** (Should · 3 pts). Como animador, quiero recorrer la pila girando el anillo de la tableta, para pasar hojas sin soltar el lápiz.
    1. El anillo se registra con tres funciones, "Girar la vista", "Zoom" y "Hojas"; el botón central alterna entre ellas y el overlay de Wacom muestra el nombre.
    2. En "Hojas", cada paso del anillo es una hoja (sentido horario = siguiente); mismas reglas que ← → (RNF-01, mesa de luz apagada mientras gira).
    3. Depende de HU-101 para la función "Zoom"; hasta entonces son dos funciones.
96. **HU-96 · Roll** (Could · 2 pts). Como animador, quiero ver las últimas hojas sin tinte manteniendo una tecla, como rolar el papel con los dedos.
    1. Mantener la tecla muestra la activa y las 3 anteriores sin tinte, con la más vieja más tenue; soltar vuelve al estado anterior.
    2. Se decide después de usar el flip en serio (idea.md, ideas abiertas): si no suma, se cierra sin hacer.

### E4 · Lápices y papel

97. **HU-97 · Papel de animación** (Must · 2 pts). Como animador, quiero un papel más blanco y de grano más fino que el de Cartuchera, sin relieve visual, para que la mesa de luz se vea limpia.
    1. La hoja de Mesa de animación tiene su color (token del tema, más cercano al blanco) y un grano más fino en la simulación; el relieve visual está en 0 y no tiene atajo.
    2. Cartuchera y Ejercicios no cambian: el papel es un parámetro del lienzo.
98. **HU-98 · Un solo grafito** (Must · 2 pts). Como animador, quiero un único grafito (HB o B) en vez de la escala 2H a 6B, para no elegir dureza.
    1. La app arranca con el grafito; la dureza (HB o B) se decide dibujando en esta historia y queda escrita en idea.md.
    2. `[ ]`, `, .` y `- =` siguen calibrando ese lápiz; Ctrl+S no existe (HU-86): los ajustes se guardan solos en `medios.json` de la app.
99. **HU-99 · Lápices de color** (Must · 5 pts). Como animador, quiero azul no-foto, rojo, verde y violeta con el trazo de un lápiz de color real, para marcar construcción, sombras y cierres con significado fijo.
    1. Cuatro lápices con color, dureza y punta propias (más cera, trazo más tenue, menos borrable que el grafito), simulados por `drymedia` como un medio distinto del grafito, no como grafito teñido.
    2. Cada color deposita en su propio canal: borrar un color no toca los otros; exportar (HU-109) conserva los colores.
    3. Prueba unitaria: el trazo de cada lápiz deposita solo en su canal; el render mezcla los canales con el orden de la lista (grafito encima).
100. **HU-100 · F5 elige entre los cinco** (Must · 2 pts). Como animador, quiero elegir el lápiz con F5 o con una tecla por lápiz, para cambiar de color sin mirar.
    1. F5 abre el selector con los cinco lápices (muestra real de cada uno); 1 a 5 eligen directo (los números quedan libres: en esta app 4/6/5 no giran la vista, el anillo y Shift + arrastrar sí).
    2. El lápiz activo se ve en F3 y queda en config.

### E5 · Vista

101. **HU-101 · Zoom** (Should · 5 pts). Como animador, quiero acercar y alejar la vista, para trabajar un detalle y volver a ver todo.
    1. `+` y `−` acercan y alejan alrededor del centro de la hoja (pasos de ~20 %, de 25 % a 400 %); `Ctrl+0` ajusta la hoja a la ventana y `Ctrl+1` vuelve a 1:1 con la tableta.
    2. La función "Zoom" del anillo hace lo mismo de forma continua.
    3. Con zoom, el trazo sigue cayendo bajo la punta (la simulación lleva la muestra a la hoja con la escala, como hoy con la rotación) y la latencia no cambia (RNF-04).
    4. Se guarda en la toma con la rotación.
102. **HU-102 · Pan** (Should · 3 pts). Como animador, quiero desplazar la vista con Espacio + arrastrar, para mirar la parte de la hoja que me quedó fuera al acercar.
    1. Mientras Espacio está apretado, arrastrar con el lápiz mueve la vista y no dibuja.
    2. `Ctrl+0` y `Ctrl+1` también centran.
103. **HU-103 · Guía de campo** (Could · 2 pts). Como animador, quiero una guía de campo de 12 campos superpuesta, para encuadrar y mantener el registro entre hojas.
    1. Un atajo la muestra y oculta; va bajo el grafito como las guías de Ejercicios, con los colores de guía del tema.

### E6 · Rol y chart

104. **HU-104 · Rol y número** (Should · 3 pts). Como animador, quiero marcar una hoja como key o breakdown y ver su número en la esquina, para leer la pila como leo el papel.
    1. Un atajo alterna sin rol → key → breakdown; la marca (número con círculo para key, con una raya para breakdown) se dibuja en la esquina superior derecha, fuera del área de dibujo, con el estilo de las guías.
    2. El número es la posición en la pila (la fija no cuenta) y se actualiza al reordenar.
    3. Se guarda en `toma.json`.
105. **HU-105 · Carta de timing** (Could · 2 pts). Como animador, quiero un rincón reservado en la hoja key para dibujar el chart a mano, para que viva con la clave.
    1. En las keys se ve un rectángulo tenue en la esquina superior izquierda; es solo una guía: ahí se dibuja con el lápiz como en el resto de la hoja.
    2. Un atajo lo muestra y oculta.

### E7 · Audio

106. **HU-106 · Audio de referencia y onda** (Should · 5 pts). Como animador, quiero asociar un audio a la toma y ver su onda, para tener la referencia a la vista.
    1. Un atajo abre el diálogo para elegir un WAV o MP3; la ruta queda en `toma.json` y el archivo se copia a la carpeta de la toma.
    2. La onda se dibuja en una franja al pie de la pantalla, fuera de la hoja, con un cursor de posición; un atajo muestra y oculta la franja.
    3. Decodificación con Qt Multimedia (ya está en la instalación de Qt); sin otra dependencia.
107. **HU-107 · Play y pausa** (Should · 2 pts). Como animador, quiero una tecla de play/pausa, para escuchar mientras flipeo a mano.
    1. Una tecla alterna play y pausa; otra vuelve al principio; el cursor de la onda sigue el audio.
    2. El audio no toca los hilos de simulación ni render (RNF-04).
108. **HU-108 · Marcas sobre la onda** (Should · 3 pts). Como animador, quiero poner marcas en el audio con una tecla, para señalar acentos y sílabas.
    1. Una tecla pone una marca en la posición actual; con el lápiz sobre la franja, un toque pone la marca ahí y arrastrarla la mueve; otra tecla borra la más cercana.
    2. Las marcas se numeran y se guardan en `toma.json`.

### E8 · Exportación

109. **HU-109 · Secuencia PNG** (Should · 3 pts). Como animador, quiero exportar la toma como PNG numerados, para llevar el rough a otra app o armar un pencil test afuera.
    1. Un atajo exporta todas las hojas (en orden, sin la fija salvo que se pida) a `<toma>/export/NNNN.png`, a la resolución de la hoja, con los colores y sin el tinte ni las guías.
    2. Las hojas con rol llevan el rol en `toma.json` exportado al lado, para la app de pencil test futura.

### E9 · Entrega

110. **HU-110 · Instalador y docs** (Must · 2 pts). Como usuario, quiero Mesa de animación en el instalador de Trazos y documentada, para probarla en la laptop.
    1. `package.ps1` incluye `mesadeanimacion.exe` con su ícono y su acceso en el menú Inicio.
    2. README, `docs/diseno.md` (teclas y mesas) y `docs/mesa-de-animacion/arquitectura.md` (formato de toma, decisiones) al día; la prueba de la app real cubre flip, Insert y mesa de luz.

## Plan por fases

Primero se mide el flip con hojas reales; recién después se construye la pila. Cada fase termina en algo que se usa para animar.

1. **Fase 0 — Factibilidad** (HU-81; 3 pts)
    1. Spike del flip y de la memoria por hoja. Salida: arquitectura de la pila decidida y escrita.
2. **Fase 1 — MVP: flipear** (HU-82 a HU-88, HU-90, HU-91, HU-97, HU-98, HU-110; 39 pts)
    1. Pila en el lienzo, tinte en el render, app, formato y guardado automático, hojas, flip, mesa de luz, papel de animación, grafito único, instalador.
    2. Salida: una toma de 24 hojas animada de punta a punta con flip y mesa de luz, guardada sola y reabierta igual.
3. **Fase 2 — Mano de animador** (HU-89, HU-92 a HU-95, HU-99 a HU-105; 33 pts)
    1. Hoja fija, cuántas hojas, solo keys, círculo, anillo con tres funciones, lápices de color, F5, zoom, pan, guía de campo, rol y número, chart.
    2. Salida: un ciclo de caminata con keys y breakdowns, construcción en azul y sombras marcadas.
4. **Fase 3 — Audio y salida** (HU-96, HU-106 a HU-109; 15 pts)
    1. Audio con onda, play/pausa y marcas; exportación PNG; el roll, si sumó.
    2. Salida: un lip sync corto animado contra el audio y exportado.

Las fases son milestones propios de la app ("Mesa de animación · Fase N"), no de Cartuchera. Las historias entran al tablero cuando su fase arranca.

## Diseño

Cada pantalla nueva tiene su mesa en el canvas de diseño de Trazos antes de implementarse: mesa de luz (tintes y opacidades), esquina con número y rol, franja de audio con onda y marcas, selector F5 con los cinco lápices. Los tokens nuevos (color del papel de animación, rojo y verde del tinte, los cuatro lápices) van a `Theme.h`.

## Decisiones abiertas

Heredadas de idea.md; cada una se cierra en la historia que la toca.

1. HB o B para el grafito (HU-98).
2. Breakdown como rol, o solo key (HU-104).
3. Si el roll suma algo con el flip rápido (HU-96).
4. Qué representación guarda una hoja no activa en RAM y cuál en disco (HU-81, HU-85).
5. Cómo deposita un lápiz de color en drymedia: medio nuevo con cera, o grafito con otro perfil de depósito y canal propio (HU-99).
6. Qué pasa con el audio al flipear: por ahora, nada.
