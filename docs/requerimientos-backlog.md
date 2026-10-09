# Ejercicios de tableta — Requerimientos y backlog

Oct 8, 2026 · @Daniel

Backlog ágil de la v1 definida en `alcance-v1.md`: 9 épicas, historias con criterios de aceptación y un plan de 8 sprints con un MVP usable al final del sprint 2.

> **Estado (9 de octubre de 2026):** los 8 sprints están cerrados y la v1.0.0 se publicó con instalador. El estado vivo de cada historia está en los [issues](https://github.com/elaqueo/ejercicios-trazos/issues) y el [tablero](https://github.com/users/elaqueo/projects/3); este documento es el plan original, con las historias que se agregaron al final.

## Enfoque ágil

Scrum liviano adaptado a un solo desarrollador: sprints de 2 semanas, cada uno cierra con un incremento usable de la app.

1. **Roles:** Daniel cumple los tres roles: Product Owner (prioriza el backlog), desarrollador y usuario final.
2. **Sprint:** 2 semanas, con un objetivo de sprint de una sola frase.
3. **Planning:** al inicio del sprint se eligen historias del tope del backlog, que deben cumplir la Definition of Ready.
4. **Review:** al cierre, se usa la app con la tableta durante una sesión real de práctica. Lo que molesta se convierte en historias nuevas.
5. **Retro:** tres preguntas, escritas en 10 minutos: qué funcionó, qué no y qué cambio para el próximo sprint.
6. **Estimación:** story points en escala Fibonacci (1, 2, 3, 5, 8). Una historia de 8 o más se divide antes de entrar a un sprint.
7. **Priorización:** MoSCoW (Must, Should, Could, Won't) dentro de cada épica.
8. **Nomenclatura:** épicas `E1`…`E9`, historias `HU-01`…; las historias técnicas (enablers) se marcan como tales.

## Visión del producto

Para Daniel, que quiere mejorar su control de trazo con tableta, la app genera ejercicios rápidos y aleatorios que se encadenan con una tecla, sin fricción entre un intento y el siguiente.

1. **Objetivo de producto:** practicar 15 tipos de ejercicio de trazo, uno por pantalla, con los pinceles de MyPaint y el lienzo rotable.
2. **Objetivo del MVP (fin del sprint 2):** abrir la app, pintar con presión sobre el área mapeada y practicar el ejercicio "dos puntos → recta" encadenando intentos con →.
3. **Objetivo de plataforma:** dejar `paintcore` y `appkit` listos para reutilizar en las próximas apps de la familia.

## Requerimientos no funcionales

La prioridad es que el trazo se sienta inmediato; el resto son restricciones de arquitectura heredadas del documento de alcance.

| ID | Requerimiento | Criterio de verificación |
| --- | --- | --- |
| RNF-01 | Latencia de trazo imperceptible | El trazo sigue al lápiz sin retraso visible a la frecuencia de refresco del monitor, incluso con trazos rápidos |
| RNF-02 | Rendimiento en ultra-wide | Pintar y rotar la vista a pantalla completa no produce saltos ni caídas de fluidez visibles |
| RNF-03 | Arranque directo | La app abre directamente en el último ejercicio usado, sin pantallas intermedias |
| RNF-04 | Fidelidad del input | Presión e inclinación de `QTabletEvent` llegan a libmypaint sin pérdida de muestras |
| RNF-05 | Aislamiento del motor | `paintcore` no expone tipos de libmypaint ni depende de `appkit` |
| RNF-06 | Reutilización | `appkit` y `paintcore` no contienen código específico de ejercicios |
| RNF-07 | Build | Monorepo CMake, bibliotecas estáticas, compila con un único comando en la PC de desarrollo |
| RNF-08 | Testabilidad | La geometría de los generadores es determinista dada una semilla y tiene pruebas unitarias |

## Épicas

El backlog inicial suma 36 historias y 103 puntos en 9 épicas; las tres primeras construyen la plataforma reutilizable.

| ID | Épica | Objetivo | Historias | Puntos | Sprints |
| --- | --- | --- | --- | --- | --- |
| E1 | Build y monorepo | Compilar todo con un comando, libmypaint integrada | HU-01 a HU-03 | 8 | 0 |
| E2 | paintcore | Pintar con pinceles MyPaint sobre un lienzo rotable | HU-04 a HU-08 | 17 | 0–1 |
| E3 | appkit | Ventana, área útil, overlays, configuración y atajos | HU-09 a HU-14 | 17 | 1–3 |
| E4 | Núcleo de ejercicios | Contrato de ejercicio, ciclo siguiente/repetir, modo mixto | HU-15 a HU-20 | 13 | 2–4 |
| E5 | Líneas y curvas | Ejercicios 1 a 5 del alcance | HU-21 a HU-25 | 12 | 2, 4 |
| E6 | Elipses | Ejercicios 6 y 7 | HU-26 a HU-27 | 6 | 5 |
| E7 | Perspectiva | Cajas, cajas rotadas y elipses en perspectiva | HU-28 a HU-31 | 16 | 6–7 |
| E8 | Escritura | Escribir sobre recta, curva y varias curvas | HU-32 a HU-34 | 6 | 5 |
| E9 | Memoria y presión | Ghosting y control de presión | HU-35 a HU-36 | 8 | 7 |

## Historias de plataforma (E1–E3)

Cada historia indica tipo, prioridad MoSCoW y estimación inicial; debajo, sus criterios de aceptación.

### E1 · Build y monorepo

1. **HU-01 · Esqueleto del monorepo** (enabler · Must · 3 pts). Como desarrollador, quiero un monorepo CMake con `libs/paintcore`, `libs/appkit` y `apps/ejercicios`, para compilar toda la familia con un comando.
   1. Un único comando configura y compila los tres targets en la PC de desarrollo.
   2. `paintcore` y `appkit` compilan como bibliotecas estáticas.
   3. La app enlaza ambas y abre una ventana vacía.
2. **HU-02 · Integrar libmypaint** (enabler · Must · 3 pts). Como desarrollador, quiero libmypaint como dependencia reproducible, para no instalarla a mano.
   1. El build resuelve libmypaint sin pasos manuales fuera del procedimiento documentado.
   2. Solo `paintcore` enlaza contra libmypaint.
3. **HU-03 · Pruebas unitarias base** (enabler · Must · 2 pts). Como desarrollador, quiero un target de pruebas con Qt Test, para validar la geometría de los generadores.
   1. Las pruebas corren con un comando y reportan pasa/falla.
   2. Existe al menos una prueba de ejemplo que pasa.

### E2 · paintcore (sin efecto desde HU-68: el motor propio la reemplazó)

4. **HU-04 · Pintar con presión e inclinación** (Must · 5 pts). Como usuario, quiero que el trazo responda a la presión y la inclinación del lápiz, para practicar con un trazo realista.
   1. Presión, inclinación y tiempo de `QTabletEvent` llegan a libmypaint en cada muestra.
   2. Un trazo rápido no muestra huecos ni segmentos rectos visibles.
   3. Con mouse se puede pintar con presión fija.
5. **HU-05 · Cargar pinceles** (Must · 3 pts). Como usuario, quiero que la app cargue los pinceles `.myb` de una carpeta, para usar los packs de MyPaint o Krita.
   1. Se cargan todos los `.myb` válidos de la carpeta configurada, compartida entre apps.
   2. Un archivo inválido se ignora y se registra en el log, sin cerrar la app.
6. **HU-06 · Elegir pincel** (Must · 3 pts). Como usuario, quiero elegir el pincel desde un selector, para cambiar de herramienta sin salir de la app.
   1. El selector lista los pinceles cargados, con su nombre.
   2. El pincel elegido se usa desde el trazo siguiente y se recuerda entre sesiones.
7. **HU-07 · Rotar la vista** (Must · 5 pts). Como usuario, quiero rotar el lienzo con una tecla modificadora y el lápiz, para dibujar en el ángulo que me resulta cómodo.
   1. Modificador + arrastre rota la vista alrededor del centro del área útil.
   2. Con snap activo, el ángulo salta de a 15°.
   3. Una tecla vuelve la rotación a 0°.
   4. Con la vista rotada, el trazo aparece exactamente bajo la punta del lápiz.
8. **HU-08 · Limpiar el lienzo** (Must · 1 pt). Como app cliente, quiero limpiar la superficie con una sola llamada, para empezar cada ejercicio en blanco.
   1. Tras limpiar, no queda ningún píxel del trazo anterior.

### E3 · appkit

9. **HU-09 · Pantalla completa en el monitor elegido** (Must · 2 pts). Como usuario, quiero que la app abra a pantalla completa en el monitor configurado, para no acomodar ventanas.
   1. La app abre fullscreen en el monitor guardado en la configuración.
   2. Si ese monitor no está conectado, abre en el principal.
10. **HU-10 · Área útil configurable** (Must · 3 pts). Como usuario, quiero definir el rectángulo de pantalla mapeado a la tableta, para que todo ejercicio quede al alcance del lápiz.
    1. El rect se define en el panel y se guarda.
    2. Fuera del rect, la pantalla queda en un tono neutro y no se dibujan ejercicios.
11. **HU-11 · Menú overlay** (Must · 3 pts). Como usuario, quiero un menú superpuesto para elegir el ejercicio o el modo mixto, sin salir de pantalla completa.
    1. Una tecla abre y cierra el menú.
    2. Elegir una opción cierra el menú y genera ese ejercicio.
12. **HU-12 · Panel de configuración** (Must · 5 pts). Como usuario, quiero un panel superpuesto para ajustar los parámetros, para no editar archivos a mano.
    1. Cada app o ejercicio declara sus parámetros (tipo, rango, valor por defecto) y `appkit` genera los controles.
    2. Los cambios se aplican desde el ejercicio siguiente.
    3. El panel incluye la sección de pincel y de área útil.
13. **HU-13 · Configuración persistente** (Must · 2 pts). Como usuario, quiero que la configuración se guarde, para no repetirla cada vez.
    1. Se guarda en JSON, con una sección por app y una común.
    2. Un archivo ausente o corrupto se reemplaza por los valores por defecto, sin cerrar la app.
14. **HU-14 · Atajos centralizados** (Should · 2 pts). Como desarrollador, quiero registrar los atajos en un solo lugar, para evitar conflictos entre app y framework.
    1. Todos los atajos se declaran en un registro único.
    2. Registrar dos acciones con la misma tecla produce un error visible al iniciar.

## Historias del núcleo de ejercicios (E4)

El núcleo define cómo se genera, muestra y encadena cualquier ejercicio; cada ejercicio concreto solo implementa su generador.

15. **HU-15 · Contrato de ejercicio** (enabler · Must · 3 pts). Como desarrollador, quiero una interfaz común de ejercicio, para agregar ejercicios nuevos sin tocar el núcleo.
    1. Un ejercicio recibe parámetros, una semilla y la zona segura, y devuelve su geometría ideal y sus guías.
    2. Con la misma semilla y los mismos parámetros, la geometría es idéntica (verificado con prueba unitaria).
    3. La geometría ideal se conserva aunque la v1 no la evalúe.
16. **HU-16 · Siguiente ejercicio** (Must · 2 pts). Como usuario, quiero pasar al siguiente ejercicio con →, para encadenar intentos sin pausa.
    1. → limpia el lienzo y genera un ejercicio nuevo con otra semilla.
    2. No existe deshacer: Ctrl+Z no tiene efecto.
17. **HU-17 · Repetir ejercicio** (Should · 1 pt). Como usuario, quiero repetir el mismo ejercicio, para reintentar un caso que me salió mal.
    1. Una tecla limpia el lienzo y regenera el ejercicio con la misma semilla.
18. **HU-18 · Zona segura y orientación aleatoria** (Must · 2 pts). Como usuario, quiero que los ejercicios aparezcan rotados al azar y nunca cortados, para no entrenar siempre el mismo ángulo.
    1. Toda guía queda dentro del círculo inscripto en el área útil, con cualquier rotación de vista.
    2. La orientación de cada ejercicio varía al azar en 360°.
    3. Los puntos de fuga pueden quedar fuera de la zona: solo sus líneas guía deben estar dentro.
19. **HU-19 · Capas de guías y trazo** (enabler · Must · 2 pts). Como desarrollador, quiero guías y trazo en capas separadas, para ocultar las guías sin afectar el trazo.
    1. Las guías se dibujan sobre el lienzo sin pasar por libmypaint.
    2. Ocultar las guías no altera los píxeles del trazo.
20. **HU-20 · Modo mixto** (Should · 3 pts). Como usuario, quiero un modo que alterne ejercicios al azar, para practicar variado sin elegir.
    1. En el panel se eligen los ejercicios que participan del modo mixto.
    2. Cada → elige al azar un ejercicio habilitado, cada uno con sus propios parámetros.
    3. El mismo tipo no se repite más de dos veces seguidas.

## Historias de ejercicios (E5–E9)

Todas comparten un criterio implícito: el ejercicio respeta la zona segura, la orientación aleatoria y los parámetros configurados en el panel.

### E5 · Líneas y curvas

21. **HU-21 · Dos puntos → recta** (Must · 2 pts). Como usuario, quiero dos puntos al azar para unirlos con un trazo recto.
    1. La distancia entre los puntos cae entre el mínimo y el máximo configurados.
22. **HU-22 · Tres puntos → curva** (Must · 3 pts). Como usuario, quiero tres puntos para unirlos con una sola curva.
    1. El punto medio se aparta de la recta entre los extremos al menos el desvío mínimo configurado.
    2. Se distinguen visualmente los extremos del punto medio.
23. **HU-23 · Hatching** (Must · 3 pts). Como usuario, quiero un área para rellenar con líneas paralelas.
    1. Se muestra el contorno del área y una línea de muestra con el ángulo y el espaciado sugeridos.
24. **HU-24 · Radiales** (Must · 2 pts). Como usuario, quiero un punto central y puntos de partida, para trazar rectas que converjan.
    1. Se muestran el centro y la cantidad configurada de puntos de partida, a la distancia configurada.
25. **HU-25 · Dirección forzada** (Must · 2 pts). Como usuario, quiero que una flecha indique el sentido del trazo, para practicar las direcciones incómodas.
    1. La flecha usa solo las direcciones habilitadas en el panel.

### E6 · Elipses

26. **HU-26 · Elipses por grado** (Must · 3 pts). Como usuario, quiero que se indique el grado y la posición de una elipse, para dibujarla.
    1. Se muestran el grado, el centro y el eje menor; el grado sale de los habilitados.
    2. El tamaño cae entre el mínimo y el máximo configurados.
27. **HU-27 · Elipses concéntricas sobre un eje** (Must · 3 pts). Como usuario, quiero un eje menor y varias posiciones, para dibujar elipses alineadas de distinto grado.
    1. Se muestran el eje y la cantidad configurada de centros, con su grado dentro del rango.

### E7 · Perspectiva

28. **HU-28 · Cajas con 1 y 2 PF** (Must · 5 pts). Como usuario, quiero un horizonte y puntos de fuga, para construir una caja.
    1. Se muestran el horizonte y 1 o 2 PF según la configuración.
    2. Con 2 PF, la separación entre ellos evita distorsiones extremas cerca del centro.
29. **HU-29 · Cajas con 3 PF y arista inicial** (Should · 3 pts). Como usuario, quiero el tercer PF y la opción de una arista de partida, para ejercicios más guiados o más difíciles.
    1. Con la opción activa, se dibuja una arista vertical inicial correcta respecto de los PF.
30. **HU-30 · Cajas rotadas** (Should · 3 pts). Como usuario, quiero dibujar la misma caja girando sobre su eje vertical, para entender cómo se mueven los PF.
    1. Para cada paso se muestran los PF correspondientes al ángulo de esa caja.
31. **HU-31 · Elipses en perspectiva** (Must · 5 pts). Como usuario, quiero un cuadrado en perspectiva, para inscribir la elipse.
    1. Se muestra un cuadrado correcto respecto de los PF configurados, sobre distintos planos.

### E8 · Escritura

32. **HU-32 · Escribir sobre una recta** (Must · 2 pts). Como usuario, quiero una línea base con su interlineado, para escribir sobre ella.
    1. Largo, ángulo e interlineado salen de la configuración.
33. **HU-33 · Escribir sobre una curva** (Must · 2 pts). Como usuario, quiero una línea base curva, para escribir siguiéndola.
    1. La curvatura no supera el máximo configurado.
34. **HU-34 · Escribir sobre varias curvas** (Must · 2 pts). Como usuario, quiero varias líneas curvas, para escribir un párrafo sobre ellas.
    1. Las curvas no se cruzan entre sí y respetan el interlineado.

### E9 · Memoria y presión

35. **HU-35 · Ghosting con memoria** (Must · 3 pts). Como usuario, quiero ver una forma unos segundos y luego reproducirla de memoria.
    1. La forma se ve durante el tiempo configurado y luego se oculta.
    2. Una tecla vuelve a mostrarla sobre el trazo para comparar.
36. **HU-36 · Control de presión** (Must · 5 pts). Como usuario, quiero seguir un perfil de presión objetivo, para controlar el grosor del trazo.
    1. Se muestra una guía con el grosor objetivo a lo largo del trazo, según el perfil elegido.
    2. Perfiles disponibles: fino → grueso, grosor constante y perfil libre aleatorio.

## Plan de sprints

Ocho sprints de 2 semanas a unos 13 puntos cada uno; el MVP se usa al cierre del sprint 2. La velocidad es una estimación inicial y se recalibra después del sprint 1.

| Sprint | Objetivo | Historias | Puntos |
| --- | --- | --- | --- |
| 0 | Pintar un trazo con libmypaint en una ventana Qt (ataca el mayor riesgo técnico primero) | HU-01, 02, 03, 04 | 13 |
| 1 | Pintar con pinceles elegibles y rotar la vista a pantalla completa | HU-05, 06, 07, 08, 09 | 14 |
| 2 · **MVP** | Practicar "dos puntos → recta" encadenando intentos con → | HU-10, 15, 16, 18, 19, 21 | 14 |
| 3 | Menú, panel y configuración persistente | HU-11, 12, 13, 14, 17 | 13 |
| 4 | Líneas y curvas completas, modo mixto | HU-20, 22, 23, 24, 25 | 13 |
| 5 | Elipses y escritura | HU-26, 27, 32, 33, 34 | 12 |
| 6 | Cajas en perspectiva | HU-28, 29, 30 | 11 |
| 7 | Elipses en perspectiva, ghosting y presión: cierre de la v1 | HU-31, 35, 36 | 13 |

En el MVP, los parámetros vienen de valores por defecto en el código; el panel llega en el sprint 3.

**Resultado:** todos los sprints se cerraron. Los sprints 3 a 7 se hicieron el 9 de octubre de 2026, ya sobre el motor propio (ver "Historias agregadas después del plan").

## Historias agregadas después del plan

Surgieron de las reviews, del diseño y del proyecto Cartuchera (motor de medios secos, `docs/medios-secos/arquitectura.md`).

| Historias | Qué | Estado |
| --- | --- | --- |
| HU-37 a HU-42 | Diseño "Taller nocturno", puntero propio, sin barra de título, rotación, hoja off-white (HU-42 quedó obsoleta) | Hechas |
| HU-43 a HU-53 | Cartuchera, fases 0 y 1: spikes de entrada, swapchain y kernel; `tabletinput`, `drymedia` (papel, punta HB, depósito), trazo en vivo, hoja = tableta, deshacer | Hechas |
| HU-56 a HU-58 | Cartuchera, fase 2: techo de tono, durezas 2H a 6B, goma | Hechas |
| HU-59 a HU-62 | Cartuchera, fase 2: costado, bruñido, línea blanca, desgaste | Pendientes |
| HU-63 a HU-68 | Ejercicios sobre el lienzo de Cartuchera; fuera paintcore, libmypaint y vcpkg | Hechas |
| HU-69 | Hoja del tamaño del mapeo de la tableta (pedido: dibujar sobre una A3) | Hecha |
| HU-70 a HU-72 | Instalador v1.0.0 con íconos, prueba de la app real, documentos al día | Hechas |

La épica E2 (`paintcore`) quedó sin efecto con el cambio de motor; HU-46 se cerró como obsoleta. Las ideas #54 (editor de medios secos) y #55 (editor de soportes) siguen abiertas.

## Definition of Ready y Definition of Done

Una historia entra al sprint solo si está lista, y se cierra solo si está terminada según estas listas.

**Definition of Ready**

1. Tiene formato "como / quiero / para" y criterios de aceptación verificables.
2. Está estimada en 5 puntos o menos.
3. Sus dependencias están terminadas o entran en el mismo sprint.
4. Los parámetros configurables están identificados, con valores por defecto.

**Definition of Done**

1. Cumple todos sus criterios de aceptación, probados con la tableta, no solo con mouse.
2. Compila sin warnings nuevos con el comando único de build.
3. La lógica de geometría tiene pruebas unitarias que pasan.
4. No introduce código específico de ejercicios en `paintcore` ni en `appkit` (RNF-05, RNF-06).
5. Está integrada en la rama principal.

## Decisiones (antes preguntas abiertas)

Las cuatro definiciones pendientes se resolvieron el 8 de octubre de 2026, durante el sprint 0.

1. **Sistema operativo:** Windows 10 con MSVC 14.44 (VS 2022 Build Tools), CMake y Ninja de las Build Tools, y Qt 6.11.2 `msvc2022_64` compartido con anim-sandbox.
2. **libmypaint:** vcpkg en modo manifiesto con un overlay del port oficial que compila sin glib, en el triplet `x64-windows-static-md-rel` (estático, CRT `/MD`, solo Release). Detalle en el README.
3. **Backlog:** GitHub Issues, una por historia, con el mismo número que la historia (`#5` = HU-05); labels para épica, prioridad MoSCoW y `enabler`; un milestone por sprint, y el tablero [Ejercicios de trazos](https://github.com/users/elaqueo/projects/3) con los campos Puntos y Prioridad. Las historias nuevas que surjan de las reviews se cargan como issues.
4. **Duración del sprint:** 2 semanas, confirmada.
