# Mesa de animación — Requerimientos y backlog

10 de octubre de 2026 · Daniel

Backlog de la app definida en [idea.md](idea.md): la tercera app de la suite, para la etapa de rough animation. Mismo enfoque ágil que [requerimientos-backlog.md](../requerimientos-backlog.md) (roles, sprints, MoSCoW, puntos Fibonacci, Definition of Ready y Done) y mismo molde que el plan por fases de Cartuchera.

> **Estado (10 de octubre de 2026):** Fase 0 cerrada (HU-81, [spikes/flip.md](spikes/flip.md)); Fase 1 en curso. Todas las historias están en los [issues](https://github.com/elaqueo/ejercicios-trazos/issues?q=label%3A%22E11+%C2%B7+Mesa+de+animaci%C3%B3n%22) y el [tablero](https://github.com/users/elaqueo/projects/3), con el milestone de su fase (se cargaron juntas para que la issue #N siga siendo la HU-N). El estado vivo está ahí; este documento es el plan.

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
| RNF-01 | Flip instantáneo | Al apretar ← o →, la hoja nueva está en pantalla en el frame siguiente: a 60 Hz, de la tecla al Present va de 0 a 16,7 ms según en qué momento del frame cae, más la composición (< 1 ms), así que menos de 18 ms; queda en el log ("Flip presentado en …"). También manteniendo la tecla apretada (autorepetición) y con el anillo |
| RNF-02 | Pila en memoria | Una toma de 120 hojas (5 segundos en unos a 24 fps) abierta entera: el estado de simulación de todas las hojas en RAM (≈ 0,6 GB a 300 dpi con solo el grafito por hoja; a 600 dpi, como estuvo entre HU-97 y HU-113, eran ≈ 2,3 GB; ver [spikes/flip.md](spikes/flip.md)) y su imagen en la GPU (≈ 7 MB por hoja), sin que el flip se degrade con el tamaño de la pila |
| RNF-03 | Mesa de luz sin demora perceptible | Al soltar la tecla, las hojas teñidas aparecen en menos de 50 ms; mientras se flipea no se compone nada |
| RNF-04 | Dibujar como en Cartuchera | La latencia del trazo sobre la hoja activa es la de Cartuchera (mediana ≈ 12 ms en F3); la pila no suma trabajo al hilo de simulación |
| RNF-05 | Guardado invisible | El guardado automático nunca frena el trazo ni el flip: se hace fuera de los hilos de simulación y render, por hoja cambiada |
| RNF-06 | Arranque directo | La app abre en la última toma, en la última hoja activa, con la vista como quedó |
| RNF-07 | Un solo lienzo | `apps/mesadeanimacion` no reimplementa nada de `libs/lienzo`; lo que la pila necesita se agrega en `lienzo` o `drymedia` y lo pueden usar las otras apps |
| RNF-08 | Formato abierto | Una toma es una carpeta con archivos que se pueden leer sin la app (JSON legible y hojas en un formato documentado) |

## Épicas

Nueve épicas, 31 historias. Las dos primeras son plataforma; el MVP es la Fase 1.

| ID | Épica | Objetivo | Historias | Fase |
| --- | --- | --- | --- | --- |
| E1 | Pila en el lienzo | Varias hojas en `lienzo`, una activa en simulación y las demás listas para mostrar; tinte en el render; perfil de medio por app | HU-81 a HU-84, HU-112 | 0–1 |
| E2 | Toma y hojas | Formato de toma, guardado automático, agregar, duplicar, borrar, reordenar, hoja fija | HU-85 a HU-89 | 1–2 |
| E3 | Flip y mesa de luz | ← →, mesa de luz rojo/verde, cuántas hojas, solo keys, círculo, anillo, roll | HU-90 a HU-96 | 1–2 |
| E4 | Lápices y papel | Papel de animación, grafito único, lápices de color, F5, menú del botón del lápiz | HU-97 a HU-100, HU-116 | 1–2 |
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
    1. `drymedia` separa el relieve (uno por toma, de solo lectura, compartido) del conjunto de tiles de depósito, que es un objeto por hoja; cambiar la hoja activa cambia de conjunto sin copiar tiles (HU-81).
    2. La simulación sostiene la pila y atiende, en su hilo, agregar, quitar, mover y activar; al activar repinta la imagen de la hoja en varios hilos (meta: ≤ 30 ms en la desktop, medido).
    3. Deshacer es por hoja: cambiar de hoja y volver conserva el deshacer de cada una.
    4. `lienzo::Lienzo` expone la pila; Cartuchera y Ejercicios siguen con una sola hoja sin cambiar su código, y el hash de una hoja dibujada con trazos fijos es el mismo que antes del cambio (prueba de regresión).
    5. Pruebas unitarias: la hoja activa recibe el trazo, las otras no; mover una hoja conserva su contenido.
83. **HU-83 · Tinte de la mesa de luz en el render** (enabler · Must · 5 pts). Como desarrollador, quiero que el render componga la hoja activa sobre hojas teñidas, para que la mesa de luz sea un modo del render y no una copia de píxeles.
    1. El render recibe hasta N hojas anteriores y M siguientes con un color y una opacidad por hoja y las compone bajo la hoja activa, en orden.
    2. Con N = M = 0 el render es el de hoy, byte a byte (prueba de regresión con hash).
    3. Cada hoja tiene su imagen en una textura de la GPU, que se actualiza al dejar de dibujar en ella; el render presenta y compone esas texturas sin copiar píxeles de la CPU durante el flip (HU-81).
    4. Componer N + M = 6 hojas de tamaño tableta tarda menos de 8 ms en la desktop.
84. **HU-84 · App Mesa de animación** (enabler · Must · 2 pts). Como usuario, quiero una app `mesadeanimacion` que abra sobre el lienzo, para empezar a usar la pila.
    1. `apps/mesadeanimacion` arranca como Cartuchera (monitor, área útil, F3, F9, F10, Ctrl+, con sus atajos) y con el nombre "Mesa de animación".
    2. Comparte config con la familia (monitor y área útil) y tiene la suya para la toma y la vista.

112. **HU-112 · Perfil de medio por app** (enabler · Must · 5 pts). Como desarrollador, quiero que el papel y el lápiz se configuren por app, para que Mesa de animación use un medio recortado sin tocar el de Cartuchera.
    1. Una hoja que no está activa guarda sus tiles solo con los planos que usa (con las funciones de ilustración apagadas, solo el depósito): 4 veces menos memoria, sin cambiar los kernels de contacto.
    2. La resolución es un dato del papel (600 o 300 dpi) en vez de una constante; el relieve se escala para que el grano conserve su tamaño físico, y la punta, el desgaste, el mapeo de pantalla y el selector F5 usan la del papel.
    3. Un perfil "animación" junta lo anterior: 300 dpi y sin bruñido, punta seca, daño de fibra ni desgaste. Cartuchera y Ejercicios siguen en 600 dpi con todo, con los mismos hashes.
    4. Medido con el spike de HU-81 adaptado: una hoja al 26 % de tiles ocupa ≤ 10 MB.
    5. *Hecho (11 oct 2026): 4,7 MB a 300 dpi. En HU-97 el usuario eligió 600 dpi probando en la app (a 300 el grano se veía grueso): el perfil quedó en 600 dpi y la hoja al 26 % en 18,4 MB.*

### E2 · Toma y hojas

85. **HU-85 · Formato de toma** (enabler · Must · 5 pts). Como usuario, quiero que una toma sea una carpeta que puedo copiar y abrir en la otra máquina, para no depender de la app para ver qué hay.
    1. Una toma es una carpeta con `toma.json` (versión, orden de hojas, hoja activa, roles, marcas de audio, ruta del audio, vista) y un archivo por hoja con el estado de la simulación (formato propio documentado en el `arquitectura.md` de la app, sin pérdida).
    2. Abrir la toma reconstruye la pila idéntica: hash de cada hoja igual al guardado (prueba unitaria con una toma de muestra).
    3. Un archivo corrupto o faltante no impide abrir la toma: la hoja queda en blanco y se avisa en el log y en F3.
    4. Análisis antes de elegir el formato (pedido del usuario, 11 oct 2026): comparar algoritmos de compresión sin pérdida para las hojas guardadas (zlib, zstd, lz4 u otros) con hojas reales a 600 dpi: tamaño en disco, tiempo de guardar y de abrir. El spike de HU-81 midió zlib nivel 1 a 8 : 1.
    5. *Hecho (11 oct 2026): el análisis está en [spikes/toma.md](spikes/toma.md). El usuario eligió zstd nivel 1, la primera dependencia fuera de Qt, compilada desde la fuente: unos 4,5 MB por hoja al 26 %, que se guarda en 39 ms y se abre en 13 ms. El formato está en [arquitectura.md](arquitectura.md). Cada hoja guarda también su imagen de pantalla, para abrir sin redibujar. Mesa de animación abre la toma al arrancar; HU-86 la guarda sola.*
86. **HU-86 · Guardado automático** (Must · 3 pts). Como animador, quiero que la toma se guarde sola, para no pensar en archivos mientras flipeo y dibujo.
    1. Cada hoja cambiada se guarda al soltar el lápiz o al cambiar de hoja, en un hilo aparte; `toma.json` al cambiar el orden, el rol o la vista.
    2. Cerrar la app con Alt+F4 espera a que termine lo pendiente (menos de 2 s con 120 hojas).
    3. No hay Ctrl+S; el atajo queda libre y la lista de atajos no lo muestra.
    4. Al arrancar se abre la última toma (RNF-06); sin ninguna, se crea "toma-1" en la carpeta de datos.
    5. *Hecho (11 oct 2026): la hoja se guarda cuando el lápiz lleva 300 ms sin dibujar (no en el instante de soltarlo, para no copiar entre trazo y trazo), y en la misma pausa `toma.json` si cambió el orden, la activa o la vista. La simulación solo copia lo que cambió; un hilo aparte lo comprime y lo escribe. La copia se corta si el lápiz vuelve a tocar el papel y se repite en la pausa siguiente: los benchmarks quedan como antes (11 ms de mediana, 0 vsyncs perdidos). Al cerrar se escribe solo lo pendiente: 0 ms en la prueba de la app real, medio segundo como máximo en el benchmark, que deja la hoja muy llena. Matar la app pierde a lo sumo lo de los últimos 300 ms. Una toma que no se puede abrir no se pisa: se sigue en la de al lado, `<carpeta>-2`, que queda como la última. Detalle en [arquitectura.md](arquitectura.md).*
87. **HU-87 · Agregar, duplicar, borrar y limpiar hojas** (Must · 5 pts, eran 3: incluye el deshacer de la pila). Como animador, quiero manejar hojas con una tecla, para que "acá falta un dibujo" no corte el ritmo.
    1. Insert agrega una hoja en blanco justo después de la activa y la deja activa.
    2. Ctrl+D duplica la activa (misma hoja, después) y Ctrl+N la limpia; Supr la borra y pide confirmación solo si la hoja tiene trazo (Supr otra vez).
    3. Todo se deshace con Z y se rehace con Ctrl+Y: Z deshace lo más reciente entre el último trazo de la hoja activa y la última operación con las hojas; deshacer un borrado devuelve la hoja a su lugar con sus trazos y su deshacer.
    4. La hoja nueva tiene el papel y el tamaño de la toma.
88. **HU-88 · Reordenar con ↑ ↓** (Must · 2 pts). Como animador, quiero mover la hoja activa un lugar con ↑ y ↓, para corregir el orden sin menú.
    1. ↑ la acerca al principio y ↓ al final; en los extremos se queda.
    2. El número de la esquina se actualiza en todas las hojas afectadas.
    3. Se puede deshacer.
    4. *Hecho (11 oct 2026): ↑ y ↓ en el grupo "Hojas"; mover es una operación más del deshacer de la pila (Z la devuelve, Ctrl+Y la rehace, en orden con los trazos) y no repinta, porque la hoja del papel no cambia. El número de la esquina llega con HU-104; hasta entonces se ve en F3. El guardado automático reescribe solo `toma.json`.*
89. **HU-89 · Hoja fija** (Should · 3 pts). Como animador, quiero una hoja debajo de todas que no participe del flip, para tener el layout o la model sheet siempre a la vista.
    1. Un atajo marca la activa como fija: pasa al fondo, sin tinte, visible con la mesa de luz prendida o apagada.
    2. ← → la saltean; ↑ ↓ no la mueven; solo puede haber una (marcar otra desmarca la anterior).
    3. Se puede dibujar en ella eligiéndola con un atajo propio (no con el flip).
    4. *Hecho (11 oct 2026): F marca y desmarca, Ctrl+F entra a dibujarla y vuelve. No se mueve de su lugar en la pila (va por id, como el rol), así no pasa por el deshacer: borrarla y deshacer la trae fija. Se ve en gris grafito al 35 %; dibujándola, la hoja de la que se vino queda tenue de referencia y la esquina dice F. La numeración no la cuenta. Va en toma.json ("fija": true). Cuesta 0,4 ms por frame (se compone siempre que hay fija).*

115. **HU-115 · Inicio y Fin saltan a la primera y la última hoja** (Must · 1 pt). Como animador, quiero saltar a la primera hoja con Inicio y a la última con Fin, para ir a un extremo de la toma sin flipear hoja por hoja (pedido del usuario del 11 de octubre de 2026).
    1. Inicio activa la primera hoja y Fin la última; si ya es la activa, no hace nada.
    2. Van a la lista de atajos (Ctrl+,), en el grupo "Hojas".
    3. *Hecho (11 oct 2026): saltar es como soltar un flip en ese extremo, así que no va al deshacer (activar una hoja tampoco). La lista de atajos ahora nombra Inicio, Fin y Tab (antes mostraba el código de la tecla).*

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
    2. *Descartada (11 oct 2026): el usuario prefiere dejar fijas 2 hojas atrás y 1 adelante; no hace falta cambiarlo.*
93. **HU-93 · Solo keys** (Should · 2 pts). Como animador, quiero que la mesa de luz muestre solo las hojas marcadas como key, para dibujar un breakdown entre dos claves sin el ruido del resto.
    1. Un atajo alterna el modo; con él, los conteos de HU-92 cuentan keys, no hojas.
    2. Depende de HU-104.
    3. *Hecho (11 oct 2026): Ctrl+L (L es la mesa de luz). Cuentan solo las keys, no los breakdowns; la activa no cuenta aunque sea key. Apagada, Ctrl+L la prende en este modo; queda en config. Sin keys alrededor no se ve nada abajo y lo dice el log. El panel de hojas dice "solo keys" (mesa "Panel de hojas") y F3 también.*
94. **HU-94 · Pila en círculo** (Should · 1 pt). Como animador, quiero que después de la última hoja venga la primera, para flipear un ciclo de caminata y probar el empalme.
    1. Un atajo lo prende y apaga; se guarda en la toma.
    2. Con el círculo, la mesa de luz también da la vuelta.
    3. *Hecho (11 oct 2026): O; "circulo": true en toma.json. El flip, la mesa de luz y la tira del panel dan la vuelta (salteando la hoja fija); la mesa de luz y la tira no repiten hojas en pilas cortas (primero las de atrás). Inicio, Fin y ↑ ↓ no dan la vuelta. El panel dice "Círculo: sí".*
95. **HU-95 · El anillo pasa hojas** (Should · 3 pts). Como animador, quiero recorrer la pila girando el anillo de la tableta, para pasar hojas sin soltar el lápiz.
    1. El anillo se registra con tres funciones, "Girar la vista", "Zoom" y "Hojas"; el botón central alterna entre ellas y el overlay de Wacom muestra el nombre.
    2. En "Hojas", cada paso del anillo es una hoja (sentido horario = siguiente); mismas reglas que ← → (RNF-01, mesa de luz apagada mientras gira).
    3. Depende de HU-101 para la función "Zoom"; hasta entonces son dos funciones.
    4. *Hecho (11 oct 2026): tercer modo de la rueda, "Hojas" (los de la Intuos4 quedan Girar la vista, Zoom, Hojas, Girar la vista). Una hoja cada 30° (seis posiciones; con una por posición, y después con dos, era demasiado sensible), como mantener la flecha: muestra sin activar, mesa de luz apagada; al levantar el dedo la que se ve queda activa. Respeta el círculo y la hoja fija.*
96. **HU-96 · Roll** (Could · 2 pts). Como animador, quiero ver las últimas hojas sin tinte manteniendo una tecla, como rolar el papel con los dedos.
    1. Mantener la tecla muestra la activa y las 3 anteriores sin tinte, con la más vieja más tenue; soltar vuelve al estado anterior.
    2. Se decide después de usar el flip en serio (idea.md, ideas abiertas): si no suma, se cierra sin hacer.
    3. *Descartada (11 oct 2026): decisión del usuario.*

### E4 · Lápices y papel

97. **HU-97 · Papel de animación** (Must · 2 pts). Como animador, quiero un papel más blanco y de grano más fino que el de Cartuchera, sin relieve visual, para que la mesa de luz se vea limpia.
    1. La hoja de Mesa de animación tiene su color (token del tema, más cercano al blanco) y un grano más fino en la simulación; el relieve visual está en 0 y no tiene atajo.
    2. El papel no se trabaja: sin bruñido, sin deformación ni daño de fibra (no hay planos para eso, HU-82). El grano sigue respondiendo al lápiz.
    3. Simulación a 300 dpi: la línea se ve igual que a 600 dpi hasta un zoom de 200 % (verificado con capturas lado a lado).
    4. Cartuchera y Ejercicios no cambian: el papel es un parámetro del lienzo.
    5. *Hecho (11 oct 2026): las capturas (`capturas/papel*.png`, `spikes/mesa-papel`) no alcanzaron para decidir; probando en la app, el usuario eligió 600 dpi y grano 900 (de 2047), color `#FAF8F2`. Corregido en HU-113: 300 dpi y grano 300.*
113. **HU-113 · Mesa de animación a 300 dpi** (Must · 1 pt). Como animador, quiero que la app trabaje a 300 dpi, porque es para rough animation: la calidad fina de la línea importa menos que el rendimiento y el espacio en disco (decisión del usuario del 11 de octubre de 2026, que corrige la de HU-97).
    1. El perfil de animación vuelve a 300 dpi, con grano 300. Cartuchera y Ejercicios no cambian.
    2. Las tomas de 600 dpi no se convierten: quedan guardadas y la app sigue en la de al lado (HU-86).
    3. Se mide de nuevo: memoria por hoja, disco, tiempo de guardar y de abrir, y los benchmarks.
    4. El usuario lo prueba en la app; si la línea de la HB se siente gruesa (a 300 dpi salía un 20 % más ancha), se ajusta el diámetro probando.
    5. *Hecho (11 oct 2026): con la hoja al 30 % de tiles, 5,4 MB en memoria (antes 18,9 al 26 %) y 1 MB en disco con la imagen (antes 4,8): el grano bajo comprime 7 : 1 en vez de 4 : 1. Guardar una hoja tarda 8 ms y abrirla 2,5 ms. 120 hojas: unos 650 MB en memoria y 115 MB en disco. Benchmark sin cambios (10,8 ms de mediana). Detalle en [spikes/toma.md](spikes/toma.md).*
98. **HU-98 · Un solo grafito** (Must · 2 pts). Como animador, quiero un único grafito (HB o B) en vez de la escala 2H a 6B, para no elegir dureza.
    1. La app arranca con el grafito; la dureza (HB o B) se decide dibujando en esta historia y queda escrita en idea.md.
    2. La punta no se gasta: sin desgaste ni afilado (no hay tecla A), así la línea de la hoja 40 es la de la hoja 1. Sin punta seca (no hay tecla E).
    3. El costado (I) queda, apagado por defecto.
    4. La goma solo levanta grafito (no hay bruñido que resista ni fibra que se dañe).
    5. `[ ]`, `, .` y `- =` siguen calibrando ese lápiz; Ctrl+S no existe (HU-86): los ajustes se guardan solos en `medios.json` de la app.
99. **HU-99 · Lápices de color** (Must · 5 pts). Como animador, quiero azul no-foto, rojo, verde y violeta con el trazo de un lápiz de color real, para marcar construcción, sombras y cierres con significado fijo.
    1. Cuatro lápices con color, dureza y punta propias (más cera, trazo más tenue, menos borrable que el grafito), simulados por `drymedia` como un medio distinto del grafito, no como grafito teñido.
    2. Los colores comparten un plano, separado del grafito: cada celda guarda qué color tiene (2 bits) y cuánto (14 bits), y donde dos colores se pisan gana el último. El grafito va en su plano, encima: borrar grafito no borra el color, como con un col-erase real.
    3. Exportar (HU-109) conserva los colores.
    4. Prueba unitaria: el grafito y los colores depositan cada uno en su plano; pintar azul sobre rojo deja azul; el render pone el grafito encima.
    5. *Hecho (11 oct 2026): decidido con el usuario que la goma borra solo lo del lápiz elegido (el grafito, o solo ese color, 4 veces más despacio; cambia el criterio 2), que la mesa de luz pasa a naranja y turquesa, y que las teclas 1 a 5 entran acá (de HU-100). Donde dos colores se pisan, la celda queda del último con la cantidad mayor de las dos. Detalle en arquitectura.md.*
100. **HU-100 · F5 elige entre los cinco** (Must · 2 pts). Como animador, quiero elegir el lápiz con F5 o con una tecla por lápiz, para cambiar de color sin mirar.
    1. F5 abre el selector con los cinco lápices (muestra real de cada uno); 1 a 5 eligen directo (hecho en HU-99: en esta app 4/6/5 no giran la vista, el anillo y Shift + arrastrar sí).
    2. El lápiz activo se ve en F3 y queda en config.
    3. *Hecho (11 oct 2026): `PencilPicker` (lienzo/LeadPicker.h), centrado sobre la hoja por pedido del usuario (tiene que quedar al alcance del lápiz: la hoja es el área útil de la tableta); las muestras van con tres pasadas y más presión, porque a 300 dpi un repaso liviano no dejaba ver el color.*
116. **HU-116 · Menú radial en el botón del lápiz** (Should · 3 pts). Como animador, quiero que el botón lateral del lápiz abra un menú alrededor de la punta, al estilo de la paleta emergente de Krita, para cambiar de lápiz y hacer lo más usado sin soltar el lápiz ni ir al teclado (pedido del usuario del 11 de octubre de 2026).
    1. Al apretar el botón lateral aparece un anillo centrado en la punta, dentro de la hoja (al alcance del lápiz): los cinco lápices con su color y, alrededor, mesa de luz sí o no, hoja anterior y siguiente, deshacer y rehacer.
    2. Se elige tocando una opción con la punta (o soltando el botón sobre ella); tocar fuera, el botón otra vez o Esc lo cierran sin hacer nada.
    3. Lo que hace cada opción es lo mismo que su tecla (pasa por el registro de atajos) y queda en el log.
    4. Diseño primero en el canvas (mesa "Menú del lápiz"); qué va en el anillo se decide ahí con el usuario.
    5. *Hecho (11 oct 2026): `PenMenu` (lienzo/PenMenu.h). En el canvas el usuario sumó "Vista inicial" (Ctrl+0: giro a 0°; con zoom y pan, HU-101 y HU-102, también la hoja entera y centrada), así que afuera hay seis acciones. Elegir un lápiz cierra; una acción deja el menú abierto. El menú es una ventana casi transparente del tamaño de la hoja, para que un toque fuera del anillo lo cierre sin dibujar; el lápiz se lee de WM_POINTER (el botón lateral en el aire). La prueba de la app real lo maneja con un lápiz simulado (scripts/pen.ps1).*

### E5 · Vista

101. **HU-101 · Zoom** (Should · 5 pts). Como animador, quiero acercar y alejar la vista, para trabajar un detalle y volver a ver todo.
    1. `+` y `−` acercan y alejan alrededor del centro de la hoja (pasos de ~20 %, de 25 % a 400 %); `Ctrl+0` ajusta la hoja a la ventana y `Ctrl+1` vuelve a 1:1 con la tableta.
    2. La función "Zoom" del anillo hace lo mismo de forma continua.
    3. Con zoom, el trazo sigue cayendo bajo la punta (la simulación lleva la muestra a la hoja con la escala, como hoy con la rotación) y la latencia no cambia (RNF-04).
    4. Se guarda en la toma con la rotación.
    5. *Hecho (11 oct 2026): opción A elegida por el usuario: la GPU agranda la imagen que ya se ve (bilineal; al alejar promedia), sin latencia extra pero suave más allá de 100 %. Por pedido de diseño el zoom va hacia la punta del lápiz si está sobre la hoja (si no, al centro), porque sin pan el centro solo mostraba el medio. Ctrl+0 vuelve todo (vista inicial) y Ctrl+1 a 100 % centrada. La rueda hace zoom en su segundo modo (el botón del centro); el modo se anota en el log para confirmarlo con la tableta del usuario. Detalle en [arquitectura.md](arquitectura.md).*
102. **HU-102 · Pan** (Should · 3 pts). Como animador, quiero desplazar la vista con Espacio + arrastrar, para mirar la parte de la hoja que me quedó fuera al acercar.
    1. Mientras Espacio está apretado, arrastrar con el lápiz mueve la vista y no dibuja.
    2. `Ctrl+0` y `Ctrl+1` también centran.
    3. *Hecho (11 oct 2026): el gesto va en la ventana del lienzo, como el giro con Shift; solo con la punta apoyada (en el aire no hace nada) y también a 100 %. Mientras Espacio está apretado, el cursor es la flecha de cuatro puntas. El corrimiento se limita para que el centro de la hoja no salga de la pantalla (también al hacer zoom hacia la punta). La prueba de la app real lo hace con el lápiz simulado y verifica que no dibujó.*
103. **HU-103 · Guía de campo** (Could · 2 pts). Como animador, quiero una guía de campo de 12 campos superpuesta, para encuadrar y mantener el registro entre hojas.
    1. Un atajo la muestra y oculta; va bajo el grafito como las guías de Ejercicios, con los colores de guía del tema.
    2. *Hecho (11 oct 2026): G, guardado en config. 12 campos en 16:9 a tamaño real (12 pulgadas de ancho; 4:3 no entra de alto en la hoja). Gris tinta al 45 % en lugar del azul de guía, que se confundía con el lápiz azul (decisión del usuario en la mesa "Guía de campo"). Va en el render, no en la imagen de la hoja: no queda en las hojas guardadas ni en la exportación; gira, se acerca y se ve al flipear. Cuesta unos 0,8 ms por frame con la guía visible (11,3 → 12,1 ms de mediana).*

114. **HU-114 · Panel de hojas** (Must · 5 pts). Como animador, quiero un panel angosto a la derecha de la pantalla con la hoja activa y sus vecinas en miniatura y los datos de la toma, para ubicarme en la pila sin abrir F3 (pedido del usuario del 11 de octubre de 2026; diseño: mesa "Panel de hojas" del canvas).
    1. En el espacio libre a la derecha de la hoja, 240 px: la toma y si está guardada; la hoja activa y el total; miniaturas de la activa y de 3 antes y 3 después, con su número; el lápiz, la mesa de luz y el giro de la vista.
    2. Tab lo muestra y lo oculta; queda guardado en config.
    3. Las miniaturas salen de las texturas de las hojas que ya están en la GPU: no frenan el flip ni el trazo (benchmarks iguales).
    4. Se actualiza al flipear, insertar, borrar, mover y deshacer.
    5. *Hecho (11 oct 2026): el panel lo pinta Qt en el hilo de la interfaz, solo cuando cambia algo, y el render lo copia encima de todo, sin girar con la vista. Cada miniatura la arma la GPU una sola vez por cambio, achicando la textura de su hoja (la activa, desde la imagen de pantalla, a lo sumo cada 250 ms mientras se dibuja) y oscureciendo el grafito 3 veces para que la línea fina se vea. Mientras se flipea, el centro de la tira es la hoja que se ve. Benchmarks iguales con el panel visible (10,8 ms de mediana) y flips en menos de 18 ms. En una pantalla baja (la laptop al 125 %) las miniaturas se achican para que entre todo. Detalle en [arquitectura.md](arquitectura.md).*

### E6 · Rol y chart

104. **HU-104 · Rol y número** (Should · 3 pts). Como animador, quiero marcar una hoja como key o breakdown y ver su número en la esquina, para leer la pila como leo el papel.
    1. Un atajo alterna sin rol → key → breakdown; la marca (número con círculo para key, con una raya para breakdown) se dibuja en la esquina superior derecha, fuera del área de dibujo, con el estilo de las guías.
    2. El número es la posición en la pila (la fija no cuenta) y se actualiza al reordenar.
    3. Se guarda en `toma.json`.
    4. *Hecho (11 oct 2026): K alterna el rol de la activa (sin deshacer; la copia de Ctrl+D sale sin rol). La marca va en gris tinta y no en el azul de las guías, que se confundía con el lápiz azul (mesa "Rol y número"); también en el panel de hojas. La pinta el render encima de la hoja, solo en su recuadro cuando no hay que componer (benchmark igual que Cartuchera). Detalle en [arquitectura.md](arquitectura.md).*
105. **HU-105 · Carta de timing** (Could · 2 pts). Como animador, quiero un rincón reservado en la hoja key para dibujar el chart a mano, para que viva con la clave.
    1. En las keys se ve un rectángulo tenue en la esquina superior izquierda; es solo una guía: ahí se dibuja con el lápiz como en el resto de la hoja.
    2. Un atajo lo muestra y oculta.
    3. *Hecho (11 oct 2026): C, guardado en config (visible por defecto). 35 × 22,5 mm a 6 mm del borde (el usuario lo achicó a la mitad y lo acercó al borde después de probar 70 × 45 a 12 mm), en gris tinta como la guía de campo (mesa "Carta de timing"). Lo dibuja la composición, no la hoja; al flipear aparece con la key de delante. Sin giro se pinta solo su recuadro, como la marca (benchmark: 11,5 ms de mediana con una key en pantalla, 11,2 sin ella).*

### E7 · Audio

106. **HU-106 · Audio de referencia y onda** (Should · 5 pts). Como animador, quiero asociar un audio a la toma y ver su onda, para tener la referencia a la vista.
    1. Un atajo abre el diálogo para elegir un WAV o MP3; la ruta queda en `toma.json` y el archivo se copia a la carpeta de la toma.
    2. La onda se dibuja en una franja al pie de la pantalla, fuera de la hoja, con un cursor de posición; un atajo muestra y oculta la franja.
    3. Decodificación con Qt Multimedia (ya está en la instalación de Qt); sin otra dependencia.
    4. *Hecho (12 oct 2026): Qt Multimedia no estaba instalado; el usuario eligió Media Foundation (viene con Windows: el instalador no crece), en libs/audio. A abre el diálogo; el archivo se copia a la toma como audio.wav o audio.mp3. W muestra la franja (mesa "Audio"): en el hueco entre la hoja y el panel, abajo; en una pantalla sin hueco, sobre el pie de la hoja (opaca: el render la copia como el panel). Sin audio dice "A elige un WAV o MP3". Leer 12 s de WAV tarda unos 250 ms, al abrir la toma.*
107. **HU-107 · Play y pausa** (Should · 2 pts). Como animador, quiero una tecla de play/pausa, para escuchar mientras flipeo a mano.
    1. Una tecla alterna play y pausa; otra vuelve al principio; el cursor de la onda sigue el audio.
    2. El audio no toca los hilos de simulación ni render (RNF-04).
    3. *Hecho (12 oct 2026): P alterna play y pausa (Espacio no: es el pan); Retroceso vuelve al principio (si sonaba, sigue desde ahí); al terminar, P empieza de nuevo. Suena por WASAPI en su propio hilo (libs/audio, audio::Player), con el formato del archivo y la conversión de Windows. El cursor sigue la posición que se escucha, a 30 cuadros por segundo. El audio se lee en un hilo aparte: la toma abre sin esperarlo y la franja dice "leyendo…".*
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
2. **Fase 1 — MVP: flipear** (HU-82 a HU-88, HU-90, HU-91, HU-97, HU-98, HU-110, HU-112 a HU-115; 51 pts)
    1. Pila en el lienzo, perfil de medio, tinte en el render, app, formato y guardado automático, hojas, flip, mesa de luz, papel de animación, grafito único, instalador.
    2. Salida: una toma de 24 hojas animada de punta a punta con flip y mesa de luz, guardada sola y reabierta igual.
3. **Fase 2 — Mano de animador** (HU-89, HU-92 a HU-95, HU-99 a HU-105, HU-116; 36 pts)
    1. Hoja fija, cuántas hojas, solo keys, círculo, anillo con tres funciones, lápices de color, F5, menú del botón del lápiz, zoom, pan, guía de campo, rol y número, chart.
    2. Salida: un ciclo de caminata con keys y breakdowns, construcción en azul y sombras marcadas.
4. **Fase 3 — Audio y salida** (HU-96, HU-106 a HU-109; 15 pts)
    1. Audio con onda, play/pausa y marcas; exportación PNG; el roll, si sumó.
    2. Salida: un lip sync corto animado contra el audio y exportado.

Las fases son milestones propios de la app ("Mesa de animación · Fase N"), no de Cartuchera.

## Diseño

Cada pantalla nueva tiene su mesa en el canvas de diseño de Trazos antes de implementarse: mesa de luz (tintes y opacidades), esquina con número y rol, franja de audio con onda y marcas, selector F5 con los cinco lápices. Los tokens nuevos (color del papel de animación, rojo y verde del tinte, los cuatro lápices) van a `Theme.h`.

## Decisiones abiertas

Heredadas de idea.md; cada una se cierra en la historia que la toca.

1. HB o B para el grafito (HU-98).
2. Breakdown como rol, o solo key (HU-104).
3. Si el roll suma algo con el flip rápido (HU-96).
4. ~~Qué representación guarda una hoja no activa en RAM y cuál en disco~~: cerrada por HU-81 ([spikes/flip.md](spikes/flip.md)). Para mostrar, cada hoja es una textura en la GPU; para dibujar, todas las hojas tienen su estado en RAM gracias al recorte del medio (punto 7) y la activa es la que simula; en disco, comprimido. El relieve es uno por toma.
5. ~~Cómo se guardan los colores~~: un plano compartido por los cuatro colores (2 bits de color, 14 de cantidad, gana el último), separado del grafito (decidido después de HU-81). Queda abierto cómo deposita la cera: medio nuevo o perfil de depósito del grafito (HU-99).
6. Qué pasa con el audio al flipear: por ahora, nada.
7. ~~Qué funciones del medio necesita la animación~~: decidido después de HU-81. Fuera, solo en esta app: bruñido, punta seca, deformación, daño de fibra, desgaste y afilado. Queda el costado, apagado por defecto. Simulación a 300 dpi (en HU-97, 600 dpi con grano 900; de vuelta a 300 con grano 300 en HU-113).
