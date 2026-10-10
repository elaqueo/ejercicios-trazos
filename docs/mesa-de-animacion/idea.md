# Mesa de animación — La idea

10 de octubre de 2026 · Daniel

> Documento de idea. No hay requerimientos ni plan: esto es para seguir refinando qué queremos hacer y, sobre todo, qué no.

## Qué es

Una tercera app de la suite, hermana de Cartuchera, para la etapa de *rough animation*: bocetar los keys y sentir el timing y el spacing. Toma la mesa de luz del animador tradicional y la replica con la misma tecnología de Cartuchera: papel con grafito simulado, lienzo de baja latencia, tableta como única superficie.

La imagen mental es simple: una pila de hojas sobre una mesa de luz. Se dibuja en la hoja de arriba, se ven por transparencia las de abajo, y con las manos se pasan las hojas para ver si el movimiento funciona. La app no hace nada que una mesa de luz no haga; solo lo hace sin papel.

El nombre es **Mesa de animación** (ejecutable `mesadeanimacion`), porque como *Cartuchera* y *Ejercicios* nombra el objeto del taller, no la etapa del proceso: el escritorio del animador, con la mesa de luz en el centro. La mesa de luz es una de sus funciones (el onion skin), no toda la app. (Se llamó *Mesa de luz* hasta el 10 de octubre de 2026.)

## Qué no es

Esto es lo que define a la app. Cada exclusión tiene un porqué, y la mayoría no son "para después" sino contrarias a la idea.

| Fuera | Por qué |
| --- | --- |
| Reproducción automática (play, loop, fps) | El timing lo prueba la mano del animador pasando hojas, como en papel. Un play lo haría la máquina. Si alguna vez hace falta un pencil test, es otra app. |
| Duración por hoja (hold, exposiciones) | Es la consecuencia de lo anterior: si no hay play, no hay cuadros. La pila es solo un orden. |
| Timeline, x-sheet | Idem. |
| Layers | Las hojas apiladas *son* el modelo. Una capa sería una hoja con otro nombre y arruinaría la metáfora. |
| Cámara, movimientos de cámara | Es de la etapa de layout o de compositing. |
| Relleno, color plano, pintura | Lápiz sobre papel. Los colores son lápices, no pinturas. |
| Vectores, selecciones, transformaciones | Igual que en Cartuchera: interfaz ausente. |
| Inbetweening automático, tweening | El animador decide el spacing a mano; el chart se dibuja. |
| Clean-up | Etapa siguiente. Acá el trazo es sucio a propósito. |
| Varias tomas por archivo | Una toma, una pila, un audio. Menos estado, menos decisiones. |

## El modelo: una toma es una pila de hojas

- **Un archivo es una toma**: una pila ordenada de hojas y, opcionalmente, un audio de referencia.
- Todas las hojas tienen **el mismo tamaño y el mismo papel**, como las hojas de un mismo block. El tamaño es el de Cartuchera: toda el área activa de la tableta.
- El papel es **de animación, no de dibujo**: más cercano al blanco que la hoja de Cartuchera y de grano más fino. **Sin relieve visual**: el grano sigue en la simulación (es lo que hace que el grafito deposite como lápiz), pero la hoja se ve lisa. El papel de animación es liso y claro para que la mesa de luz lo atraviese bien; y debajo de hojas teñidas de rojo y verde, el relieve sería ruido, no sensación. Acá el "se siente como papel" viene del flip, no de la hoja.
- La hoja **no tiene duración**. Lo único que tiene es su posición en la pila. Esa posición es su número, y se ve en la esquina como el número que el animador escribe a lápiz.
- Una hoja es **estado de simulación**, no una imagen: grafito por celda y por color, como en Cartuchera. Por eso al reabrir la toma se sigue pudiendo borrar y redibujar como si nunca se hubiera cerrado.
- Cada hoja tiene un **rol**, opcional: *key* o *breakdown*. Un atajo lo alterna. Se dibuja la marca en la esquina como en papel (la clave con su círculo). Sirve para numerar como se numera a mano y para la mesa de luz "solo keys" (ver más abajo). Si breakdown resulta de más, queda solo key.

## El flip: el corazón de la app

Con `←` y `→` se pasa a la hoja anterior o siguiente. Es la herramienta principal de timing: el animador flipea rápido, mira, flipea otra vez. Por eso:

- **Tiene que ser instantáneo.** Toda la pila vive en RAM y cambiar de hoja es cambiar la imagen que se presenta, nada más. No se compone nada, no se lee de disco, no se recalcula.
- **El onion skin se apaga solo mientras se flipea** y vuelve al soltar. En papel, al pasar hojas se ve una hoja sola; la mesa de luz se usa cuando se dibuja. Esto además garantiza lo anterior: el flip es un cambio de imagen puro.
- `↑` y `↓` mueven la hoja activa un lugar en la pila. Reordenar es parte de animar: una hoja que estaba antes resulta que va después.
- `Insert` agrega una hoja en blanco justo después de la activa. Es el gesto más común: "acá falta un dibujo".
- Además: duplicar la hoja activa (para arrancar el siguiente dibujo desde el anterior), borrarla y limpiarla. Deshacer, como en Cartuchera, pero por hoja.
- **El anillo también pasa hojas.** Es la tercera función del anillo, *Hojas*: girarlo recorre la pila como un jog, el gesto más parecido a pasar hojas con los dedos, y sin soltar el lápiz. Mismas reglas que las flechas: instantáneo y sin mesa de luz.
- **Pila en círculo** (se prende y apaga): después de la última hoja viene la primera. Para un ciclo de caminata, lo que se quiere probar al flipear es justamente el empalme.
- **Hoja fija.** Una hoja que va debajo de todas, sin tinte, y no participa del flip ni del orden: el layout, el fondo o una model sheet. En papel es la hoja que queda debajo de las clavijas. No es un layer: es una sola, no se anima y no se apila.

## La mesa de luz (onion skin)

Un atajo la prende y apaga. Cuando está prendida:

- Las hojas **anteriores se ven teñidas de rojo** y las **siguientes de verde**, la convención tradicional, con desvanecimiento según la distancia a la hoja activa.
- Atajos para cuántas hojas hacia atrás y hacia adelante.
- Un modo **"solo keys"** que muestra únicamente las hojas marcadas como key, para dibujar un breakdown entre dos claves sin el ruido del resto.

## Los lápices

El motor es el de Cartuchera, pero la cartuchera es otra. No hay escala de minas 2H a 6B: en animación se trabaja con **un solo grafito**, HB o B (a decidir probando), y lo que cambia de lápiz a lápiz es el color, no la dureza. Hay **pocos colores y cada uno con un significado fijo**. No es una paleta: cada color es un lápiz distinto que se usa para una cosa.

| Lápiz | Para qué |
| --- | --- |
| Grafito (HB o B) | El dibujo. |
| Azul no-fotográfico | Construcción: volúmenes, ejes, líneas de acción. |
| Rojo | Sombra 1, a la manera del anime (líneas de sombra, 影). |
| Verde | Sombra 2 o highlight. |
| Violeta | Líneas de cierre de geometrías abiertas: cierran una forma para que una futura app de fill la reconozca como región y descarte la línea. |

Los lápices de color se simulan con **la dureza y el tamaño de punta de los reales** (un col-erase o un lápiz de color de animación: más cera, menos grafito, trazo más tenue y menos borrable que el HB), no como un grafito pintado de otro color. Goma como en Cartuchera.

Que cada color signifique una cosa es lo que permite que una app posterior lea el dibujo sin adivinar.

## La vista

Cartuchera ya gira la vista con la rueda de la tableta y con las teclas `4`, `6` y `5`. Mesa de animación agrega **zoom y pan**, porque al animar se trabaja un detalle y después se vuelve a ver todo.

- El **anillo** de la tableta se registra con tres funciones, *Girar la vista*, *Zoom* y *Hojas*, y el botón central del anillo alterna entre ellas, como con cualquier otra app que use el anillo.
- Atajos de zoom, los habituales en este tipo de programas: `+` y `−`, `Ctrl+0` ajusta la hoja a la ventana, `Ctrl+1` vuelve a 1:1, `Ctrl+rueda` del mouse si lo hay.
- Pan: `Espacio` + arrastrar con el lápiz.
- Sin textura visual del papel ni luz (ver el papel, más arriba).
- Una **guía de campo** (field guide, 12 campos) superpuesta y apagable, para encuadrar y mantener el registro entre hojas.

## La carta de timing

En papel, el animador dibuja el chart de spacing en un rincón de la hoja clave. Acá igual: un rincón de la hoja key reservado para dibujar el chart a mano. **No tiene lógica**: no genera nada, no calcula nada. Es dibujo que vive con la key y que se ve cuando esa key está arriba.

## El audio

Una toma puede tener un audio de referencia (diálogo, música, un acento).

- Se ve la **onda** del audio.
- Una tecla hace **play y pausa**.
- Con un atajo se ponen **marcas** sobre la onda (un acento, una sílaba, un golpe).
- **No hay relación entre el tiempo del audio y las hojas.** La sincronía la hace el animador a mano: escucha, flipea, siente dónde cae cada cosa. Es deliberado y es coherente con no tener play de la animación.

Se discutió y se dejó afuera, por ahora, que al flipear suene el pedacito de audio de cada hoja (el scrub de la moviola). Requeriría que la hoja tuviera un tiempo, y no lo tiene.

## Los archivos

- **Formato propio para la hoja**, porque lo que hay que guardar es el estado de la simulación (grafito por celda, por color), no una imagen. Si es comprimido sin pérdida o crudo es un detalle; lo importante es que no se pierde nada.
- Un **JSON** con el orden de las hojas, sus roles, las marcas de audio y la ruta del audio.
- **Guardado automático, sin Ctrl+S.** Una toma es como un block: siempre está como la dejaste. Guardar a mano es un concepto de archivo, no de mesa de luz, y en el flujo de flipear y dibujar uno se olvida.
- **PNG solo para exportar**: una secuencia numerada para llevar el rough a otra app o armar un pencil test afuera.

Sobre PNG, para que quede dicho: es sin pérdida, los píxeles salen idénticos (la compresión es como un zip). Lo que no sirve es como formato nativo, porque una imagen no guarda la simulación. Y el flip no depende del formato en disco: la pila está toda en RAM desde que se abre la toma; lo único que PNG haría más lento es abrir, no flipear.

## Atajos (provisorios)

Todo lo que no se nombra acá es como en Cartuchera (goma, tamaño, calibración, F3, F4). F5 elige el lápiz, pero entre los cinco de esta app, no entre minas.

| Tecla | Qué hace |
| --- | --- |
| `←` `→` | Hoja anterior / siguiente (flip; apaga la mesa de luz mientras tanto) |
| `↑` `↓` | Mueve la hoja activa un lugar en la pila |
| `Insert` | Hoja nueva después de la activa |
| (a definir) | Duplicar, borrar, limpiar la hoja; key/breakdown; pila en círculo; hoja fija |
| (a definir) | Mesa de luz on/off; hojas hacia atrás / adelante; solo keys |
| `+` `−` | Zoom |
| `Ctrl+0` / `Ctrl+1` | Ajustar a la ventana / 1:1 |
| `Espacio` + arrastre | Pan |
| `4` `6` `5` | Girar / a 0°, como hoy |
| Anillo | Girar la vista / Zoom / Hojas, según la función elegida con el botón central |
| (a definir) | Play/pausa del audio; marca en la onda; guía de campo |

**Para la mano izquierda.** Si se dibuja con la derecha, las flechas quedan lejos. Las teclas oficiales son estas, y la configuración recomendada es mandar desde los ExpressKeys de la tableta (se configuran en el driver de Wacom) las que más se usan: `←`, `→`, `Insert` y la mesa de luz. No hay que programar nada; solo documentarlo.

## Ideas abiertas

- ¿Breakdown como rol, o solo key?
- ¿HB o B? Se decide dibujando.
- **Roll**: mantener una tecla apretada y ver las últimas tres a cinco hojas sin tinte, como rolar el papel con los dedos. Puede que con el flip rápido y la mesa de luz que se apaga sola no agregue nada; se prueba después de tener el flip.
- ¿El chart de timing tiene atajo propio (un rincón que se agranda) o es simplemente una zona de la hoja?
- ¿Qué pasa con el audio al flipear? Por ahora, nada.
- La app de reproducción / pencil test y la app de fill son otras apps; qué formato comparten con Mesa de animación es lo único que habría que acordar.

## Qué se reutiliza y qué habría que hacer

Solo a nivel idea, para medir el tamaño.

**Se reutiliza tal cual:** el motor de medios secos (`libs/drymedia`), el lienzo de baja latencia (`libs/lienzo`), la entrada del lápiz y el anillo (`libs/tabletinput`), calibración, configuración, atajos, menú y tema (`libs/appkit`), la textura del papel y el overlay F3.

**Es nuevo:** la pila de hojas en memoria con cambio instantáneo; el papel de animación (más blanco, grano fino, sin relieve visual); el grafito único y los lápices de color con dureza y punta propias; el tinte rojo/verde de la mesa de luz en el render y la hoja fija; la tercera función del anillo; el guardado automático; zoom y pan en la vista (hoy solo gira); la lectura del audio, su onda y sus marcas; el formato de archivo de la toma y la exportación a PNG.
