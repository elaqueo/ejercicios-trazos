# Bocetos — Requerimientos y backlog

13 de octubre de 2026 · Daniel

Backlog de la app definida en [idea.md](idea.md): la quinta app de la suite, la biblioteca de cuadernos de bocetos que se llenan dibujando en Cartuchera. Mismo enfoque ágil que [requerimientos-backlog.md](../requerimientos-backlog.md) (roles, sprints, MoSCoW, puntos Fibonacci, Definition of Ready y Done) y mismo molde que los [requerimientos de Ejercicios de animación](../ejercicios-animacion/requerimientos.md). La numeración sigue la de la suite: HU-132 en adelante.

> **Estado (13 de octubre de 2026):** la Fase 1 está cerrada (HU-132 a HU-139 y HU-144). Todas las historias están en los [issues](https://github.com/elaqueo/ejercicios-trazos/issues?q=label%3A%22E13+%C2%B7+Bocetos%22) (#132 a #145, la issue #N es la HU-N) y el [tablero](https://github.com/users/elaqueo/projects/3), con el milestone de su fase. El estado vivo está ahí; este documento es el plan.

## Visión y MVP

Para Daniel, que dibuja en Cartuchera: llevar cuadernos de bocetos por tema, llenarlos de a una hoja y volver a recorrerlos, como uno de papel.

1. **Objetivo de producto:** una biblioteca de cuadernos (por colección) donde cada hoja se dibuja en Cartuchera, queda definitiva al terminarla y se recorre con PgUp y PgDn.
2. **Objetivo del MVP (fin de la Fase 1):** abrir Bocetos, crear "Bocetos diarios" con su papel y 40 hojas, apretar D, dibujar la hoja 1 en Cartuchera, terminarla con Ctrl+Enter y verla en el cuaderno; cerrar Cartuchera sin terminar la hoja 2 y retomarla después; recorrer el cuaderno con PgUp y PgDn.
3. **Objetivo de plataforma:** que el formato del cuaderno viva en una biblioteca chica (`libs/notebooks`) que usen Bocetos y Cartuchera, y que Bocetos no dependa del lienzo.

Decisiones firmes que acotan el backlog (ver idea.md, "Qué no es"): no edita hojas, no borra hojas sueltas, no puntúa ni hace estadísticas, sin nube ni compartir, no anima.

## Requerimientos no funcionales

| ID | Requerimiento | Criterio de verificación |
| --- | --- | --- |
| RNF-01 | Abre al instante | La biblioteca con 30 cuadernos se ve en menos de 500 ms desde que arranca la app (las tapas no cargan imágenes de hojas) |
| RNF-02 | Pasar hojas sin espera | PgDn y PgUp muestran la hoja siguiente en menos de 100 ms (la vecina se precarga); queda en el log |
| RNF-03 | Nada se pierde | Una hoja terminada nunca se reescribe; el borrador se guarda solo (el autoguardado de la toma) y sobrevive a que Cartuchera se cierre de golpe |
| RNF-04 | Formato versionado | `cuaderno.json` lleva versión; un cuaderno de una versión nueva no se abre con una vieja (se avisa), y uno viejo se lee con la nueva |
| RNF-05 | Cartuchera sola no cambia | Sin `--hoja`, Cartuchera es exactamente la de hoy (no guarda, mismas teclas); su latencia y sus benchmarks no cambian |
| RNF-06 | Liviana | Bocetos no enlaza `libs/lienzo` ni necesita la tableta ni Direct3D: muestra PNG |

## Épicas

Cuatro épicas, 9 historias en el MVP y 5 para después.

| ID | Épica | Objetivo | Historias | Fase |
| --- | --- | --- | --- | --- |
| E1 | Plataforma | El formato del cuaderno, el modo cuaderno de Cartuchera, la app | HU-132 a HU-134 | 1 |
| E2 | Biblioteca | La grilla de cuadernos por estantes y administrarlos | HU-135, HU-136 | 1 |
| E3 | Cuaderno | Recorrer el cuaderno, dibujar la siguiente hoja y mostrarla en el TV | HU-137, HU-138, HU-144, HU-145 | 1–2 |
| E4 | Entrega y más | Instalador y docs; exportar, Krita, miniaturas | HU-139 a HU-143 | 1–2 |

## Historias

Cada historia indica tipo, prioridad MoSCoW y estimación; debajo, sus criterios de aceptación. Las teclas son las de idea.md.

### E1 · Plataforma

132. **HU-132 · El formato del cuaderno** (enabler · Must · 3 pts). Como desarrollador, quiero el cuaderno y sus hojas en una biblioteca, para que Bocetos y Cartuchera lean y escriban lo mismo.
    1. `libs/notebooks` (sin Qt Widgets ni lienzo; QtGui solo por `QColor`): leer y escribir `cuaderno.json` (nombre, colección, color de tapa, papel, máximo de hojas, fecha de inicio, última actualización, versión) y listar los cuadernos de la carpeta de datos.
    2. El estado de cada hoja sale de su carpeta `hoja-NNNN/`: vacía (no existe), borrador (hay toma sin `terminada.png`) o terminada (hay `terminada.png`). "La siguiente hoja" es el borrador o la primera vacía; ninguna si el cuaderno está lleno.
    3. La carpeta de datos es `Documentos\Trazos\Bocetos` (decisión 1); la carpeta de un cuaderno sale de su nombre, sin chocar con otra.
    4. Pruebas: ida y vuelta del json, versión nueva rechazada, estados de hoja, la siguiente hoja con borrador, sin borrador y lleno, nombres que chocan.
133. **HU-133 · Cartuchera en modo cuaderno** (Must · 5 pts). Como dibujante, quiero que Cartuchera abra una hoja de un cuaderno, la guarde sola y la termine con una tecla, para dibujar en el cuaderno sin pasos de más.
    1. `cartuchera --hoja <carpeta>` abre la toma de esa hoja (`useTake`, una sola hoja), con el papel del cuaderno (color, grano y textura de `cuaderno.json`), y guarda sola mientras se dibuja; si no existe, la crea.
    2. **Ctrl+Enter** termina: exporta la hoja a `terminada.png` (600 dpi, sin guías, `exportTake`), actualiza la fecha del cuaderno y cierra Cartuchera. Si la hoja está en blanco, pide confirmación (la confirmación de la suite, HU-87).
    3. Cerrar sin terminar deja el borrador tal cual; volver a abrirlo lo retoma, con deshacer desde ahí.
    4. Un aviso discreto dice dónde se dibuja ("Bocetos diarios · hoja 13") y que Ctrl+Enter termina; Ctrl+, lo lista.
    5. Una hoja terminada no se abre para editar: `--hoja` sobre una terminada sale con un mensaje en el log.
    6. Sin `--hoja`, nada cambia (RNF-05; la prueba de la app real de Cartuchera igual).
    7. Diseño primero en el canvas (mesa "Cartuchera en modo cuaderno").
134. **HU-134 · App Bocetos** (enabler · Must · 2 pts). Como usuario, quiero una app `bocetos` con el tema de la suite, para empezar a armar la biblioteca.
    1. `apps/bocetos` arranca en una ventana común en el monitor de la suite (no a pantalla completa, pedido del 13 oct 2026; como las demás: F10 cambia de monitor, Ctrl+, los atajos, Ctrl+Q sale), con el tema "Taller nocturno" y su log.
    2. Comparte config con la familia (monitor) y tiene la suya (sección `bocetos`); no enlaza `libs/lienzo` (RNF-06).
    3. Encuentra `cartuchera.exe` junto a su propio ejecutable.

### E2 · Biblioteca

135. **HU-135 · La biblioteca** (Must · 5 pts). Como dibujante, quiero ver mis cuadernos como libros en estantes, para elegir en cuál dibujar.
    1. Un estante por colección (el general primero, después por nombre) con sus cuadernos como libros apaisados.
    2. La tapa: el color, el nombre, las hojas usadas sobre el máximo (en el canto), la fecha de inicio y la de la última actualización; una marca si tiene borrador y otra si está lleno.
    3. Dentro de cada estante, por última actualización (decisión abierta 3).
    4. Flechas para moverse (entre estantes también), Enter o tocar con el lápiz o el mouse abre el cuaderno. Sin cuadernos, la pantalla invita a crear el primero (Ctrl+N).
    5. Cumple RNF-01. Diseño primero en el canvas (mesa "Bocetos · biblioteca").
136. **HU-136 · Crear y administrar cuadernos** (Must · 5 pts). Como dibujante, quiero crear, renombrar, mover y borrar cuadernos y colecciones, para ordenar mi biblioteca.
    1. **Ctrl+N** abre "Cuaderno nuevo": nombre, colección, color de tapa (una paleta corta), papel (un desplegable con los papeles, cada uno con su muestra de color, más grano y textura y una muestra grande) y máximo de hojas (por defecto 40).
    2. Los papeles: crema, blanco, **kraft** (papel madera, más grano) y **tonado tostado** y **tonado gris** (como el Strathmore Toned); cada uno es un color y un grano de partida para `drymedia::PaperSpec`, y el grafito se ve bien sobre todos.
    3. **F2** edita el elegido: nombre, colección y tapa (el papel y las hojas se ven pero no cambian); **Ctrl+M** abre lo mismo con el foco en la colección. Una colección nueva se crea escribiéndola en el desplegable (editable); no hay Ctrl+Shift+N, porque una colección vacía no tendría estante (decidido el 13 oct 2026).
    4. **Supr** borra el cuaderno entero, con la confirmación de la suite (dice cuántas hojas tiene), mandándolo a la Papelera de reciclaje (decidido el 13 oct 2026); no hay borrar hojas sueltas.
    5. Un cuaderno lleno ofrece "Seguir en uno nuevo": crea "Nombre 2" con el mismo papel, color y colección.
    6. Diseño primero en el canvas (mesa "Bocetos · cuaderno nuevo").

### E3 · Cuaderno

137. **HU-137 · Recorrer el cuaderno** (Must · 3 pts). Como dibujante, quiero recorrer un cuaderno hoja por hoja, para ver lo que dibujé.
    1. Al abrirlo se ve la última hoja terminada ocupando la ventana (el PNG, ajustado); sin hojas terminadas, la primera en blanco con la invitación a dibujar.
    2. **PgDn** y **PgUp** pasan de hoja, **Inicio** y **Fin** van a la primera y a la última, **+** y **−** acercan y alejan (con arrastre para mover), **Esc** vuelve a la biblioteca.
    3. Al pie: el nombre del cuaderno, la hoja sobre el total y la fecha en que se terminó.
    4. Cumple RNF-02. Diseño primero en el canvas (mesa "Bocetos · cuaderno abierto").
138. **HU-138 · Dibujar la siguiente hoja** (Must · 3 pts). Como dibujante, quiero abrir la siguiente hoja en Cartuchera desde el cuaderno, para llenarlo en orden.
    1. **D** (en la biblioteca o en el cuaderno) lanza Cartuchera con `--hoja` sobre la siguiente hoja (el borrador o la primera vacía) y espera a que se cierre; mientras, Bocetos queda detrás sin hacer nada.
    2. Al volver, Bocetos relee el cuaderno: si la hoja quedó terminada la muestra; si quedó en borrador, lo marca.
    3. En un cuaderno lleno, D no lanza nada y ofrece "Seguir en uno nuevo" (HU-136).
    4. Si Cartuchera no está o no arranca, lo dice y queda en el log.
    5. **F11** pone Bocetos a pantalla completa y la vuelve a la ventana común (pedido del usuario del 13 oct 2026); queda para la próxima vez. Esc no sale de pantalla completa (en el cuaderno vuelve a la biblioteca).

Agregada el 13 de octubre de 2026, a pedido del usuario:

144. **HU-144 · La barra del cuaderno a la derecha** (Should · 2 pts). Como dibujante, quiero la información y las teclas del cuaderno abierto en una barra vertical a la derecha, para que la hoja (apaisada) aproveche toda la altura de la ventana (pedido del usuario del 13 oct 2026).
    1. El pie de HU-137 pasa a una barra vertical fija a la derecha: arriba el nombre del cuaderno, la hoja ("hoja 7 de 12", en ámbar), la fecha en que se terminó y lo usado ("12 de 40 usadas · borrador en la 13"); abajo, las teclas una debajo de la otra.
    2. La hoja ocupa el resto, ajustada a lo alto y lo ancho; el zoom y el arrastre (HU-137) quedan dentro de esa área.
    3. Diseño primero en el canvas (la mesa "Bocetos · cuaderno abierto", al día).

### E4 · Entrega

139. **HU-139 · Instalador, docs y prueba de la app real** (Must · 2 pts). Como usuario, quiero Bocetos en el instalador de Trazos y documentada, para usarla en la laptop.
    1. Ícono propio (`make-icons.ps1`, mesa "Íconos"), `package.ps1` y `trazos.iss` incluyen `bocetos.exe` con su acceso en el menú Inicio y el escritorio.
    2. README, `docs/diseno.md` (teclas y mesas) y una `arquitectura.md` de la app (el formato del cuaderno, el viaje a Cartuchera) al día.
    3. La prueba de la app real (`smoke.ps1`) crea un cuaderno en una carpeta temporal, abre una hoja en Cartuchera, la termina con Ctrl+Enter y la recorre con PgUp y PgDn.

### Para después (Fase 2)

Se estiman y se refinan cuando la Fase 1 esté en uso. Primero, agregada el 13 de octubre de 2026 a pedido del usuario:

145. **HU-145 · Mostrar la hoja en el TV** (Should · 8 pts; estimada en 5, re-estimada al planearla: la biblioteca nueva, el TV de mentira y la prueba de la app real). Como dibujante, quiero mandar la hoja que estoy mirando al TV del living (un Samsung en el mismo wifi), para verla grande y mostrarla (pedido del usuario del 13 oct 2026).
    1. En el cuaderno abierto, **T** busca los TVs de la red (DLNA/UPnP: SSDP, la descripción del dispositivo y su servicio AVTransport) y manda la hoja; si hay más de uno, se elige de una lista. El último TV queda en la config y la próxima vez va directo.
    2. Mientras está conectado, pasar de hoja (RePág, AvPág, Inicio, Fin) la cambia también en el TV; T otra vez, o volver a la biblioteca, lo corta. La barra del cuaderno (HU-144) dice a qué TV se manda.
    3. La hoja va como JPEG de hasta 4096 px por lado (el límite de DLNA `JPEG_LRG`), servida por un servidor HTTP chico de Bocetos en un puerto fijo (el firewall de Windows lo pregunta una vez); el TV la pide por ahí.
    4. En C++ con Qt Network (`QUdpSocket`, `QTcpServer`, `QNetworkAccessManager`), sin Python: el protocolo es el de `pdfcast`, probado con el Samsung QN85. Va en una biblioteca propia (`libs/cast`), para que la usen otras apps de la suite (el pencil test de la Mesa, por ejemplo).
    5. Si no encuentra el TV, o el TV no responde, lo dice y queda en el log. Pruebas: el mensaje de búsqueda, leer la descripción, el SOAP y el servidor HTTP contra un TV de mentira en la misma máquina; la prueba de la app real usa ese TV de mentira (`--tv`), nunca el de verdad.

El resto, según lo que pida el uso:

140. **HU-140 · Exportar una hoja** (Could · 2 pts). Como dibujante, quiero guardar la hoja que estoy mirando como PNG donde elija, para compartirla.
141. **HU-141 · Exportar el cuaderno como PDF** (Could · 3 pts). Como dibujante, quiero el cuaderno entero en un PDF, con la tapa y una hoja por página, para mostrarlo o imprimirlo.
142. **HU-142 · Cuadernos de Krita** (Could · 8 pts; estimada en 5, re-estimada al planearla: el papel lo arma Cartuchera, el `.kra` se escribe a mano y la prueba de la app real necesita un Krita de mentira). Como dibujante, quiero cuadernos cuyas hojas se dibujan en Krita, con el mismo papel, para usar sus pinceles.
    1. Al crear el cuaderno se elige la app (Cartuchera o Krita) y no cambia: un cuaderno es de una sola app.
    2. Se genera una vez `papel.png`, el papel del cuaderno con su textura. Lo arma Cartuchera (`cartuchera --papel <cuaderno>`, sin ventana), porque la textura visible es del lienzo y Bocetos no lo enlaza (RNF-06); a un cuarto de la resolución de la hoja, y Krita lo agranda. Cada hoja es un `.kra` con ese papel como capa de fondo bloqueada y una capa vacía encima.
    3. D lanza `krita <hoja.kra>`; al volver, la imagen sale del `mergedimage.png` del `.kra`. La hoja queda en borrador (se ve en el cuaderno) y se termina desde Bocetos (Ctrl+Enter en el cuaderno, con confirmación).
143. **HU-143 · Miniaturas** (Could · 2 pts). Como dibujante, quiero ver las últimas hojas de un cuaderno al pararme sobre él en la biblioteca, para reconocerlo sin abrirlo.

## Plan por fases

1. **Fase 1 — MVP: llenar y recorrer un cuaderno** (HU-132 a HU-139 y HU-144; 30 pts)
    1. El formato, Cartuchera en modo cuaderno, la app, la biblioteca, administrar cuadernos, recorrer, dibujar la siguiente hoja, la barra del cuaderno a la derecha, instalador.
    2. Salida: un cuaderno "Bocetos diarios" con varias hojas dibujadas y terminadas en Cartuchera, recorrido con PgUp y PgDn.
2. **Fase 2 — Compartir y más apps** (HU-145 y HU-140 a HU-143; 23 pts)
    1. Primero la hoja en el TV (HU-145); después exportar, cuadernos de Krita y miniaturas, según lo que pida el uso.

Las fases son milestones propios de la app ("Bocetos · Fase N"). El orden dentro de la Fase 1 es el numérico: primero la plataforma (HU-132 a HU-134), después la biblioteca y el cuaderno; HU-144 va después de HU-138 y antes del instalador (HU-139), para que la primera entrega ya tenga la barra a la derecha.

## Diseño

Cada pantalla nueva tiene su mesa en el canvas de diseño de Trazos antes de implementarse, en una fila propia "Bocetos": la biblioteca, el cuaderno abierto, el cuaderno nuevo y Cartuchera en modo cuaderno. La confirmación y la página de atajos son las de la suite; si hace falta un token nuevo (los colores de tapa), va a `Theme.h`.

## Decisiones abiertas

Cada una se cierra en la historia que la toca.

1. ~~Dónde viven los cuadernos~~: en `Documentos\Trazos\Bocetos`, porque son dibujos del usuario y así entran en sus backups (HU-132, 13 oct 2026).
2. Las teclas de dibujar y terminar: propuesta `D` y `Ctrl+Enter` (HU-133, HU-138).
3. ~~El orden dentro de un estante~~: por última actualización, la más reciente primero (HU-135, 13 oct 2026).
4. ~~La paleta de colores de tapa~~ (`theme::kTapas`, HU-136): seis tonos apagados que se leen con texto claro, terracota `#A9533D`, ocre `#A87A2A`, oliva `#5B7646`, pizarra `#3F5F80`, ciruela `#714765` y grafito `#4A5059` (HU-136, mesa "Bocetos · cuaderno nuevo").
