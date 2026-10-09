# Ejercicios de tableta — Alcance v1

> **Nota (HU-68):** desde la decisión del 10 de octubre de 2026 las dos apps corren sobre `libs/lienzo` y el motor propio `libs/drymedia` (ver `docs/medios-secos/arquitectura.md`). libmypaint, los pinceles `.myb` y `libs/paintcore` salieron del proyecto; lo que sigue sobre ellos queda como registro del alcance original.

Oct 8, 2026 · @Daniel

## Objetivo

App de escritorio personal para practicar control de trazo con tableta digitalizadora: genera ejercicios al azar, uno por pantalla, y con flecha derecha limpia el lienzo y genera el siguiente. Sin evaluación ni historial en la v1: los ejercicios son efímeros y la autoevaluación es visual.

## Plataforma y stack

Desktop nativo en C++ con Qt 6, motor de pinceles libmypaint, uso exclusivamente personal.

1. **Lenguaje:** C++ / Qt 6.
2. **Input:** `QTabletEvent` (presión, inclinación, rotación del lápiz).
3. **Pinceles:** libmypaint, con selección entre varios pinceles `.myb` (compatibles con los packs de MyPaint/Krita).
4. **Persistencia:** solo la configuración (JSON o INI). No se guardan trazos.
5. **Distribución:** ninguna; sin empaquetado ni pulido de release.

## Arquitectura

Esta es la primera de una familia de apps: el motor de pinceles y la infraestructura común viven en bibliotecas estáticas compartidas, y cada app aporta solo su lógica propia.

```text
┌───────────────────────┐ ┌───────────────────────┐ ┌───────────────────────┐
│ apps/ejercicios       │ │ app futura            │ │ app futura            │
└───────────┬───────────┘ └───────────┬───────────┘ └───────────┬───────────┘
            └─────────────────────────┼─────────────────────────┘
                                      ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ libs/appkit · ventana, área útil, overlays, config JSON, atajos         │
└─────────────────────────────────────┬────────────────────────────────────┘
                                      ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ libs/paintcore · wrapper libmypaint, pinceles .myb, superficie, input,   │
│                  widget de lienzo con rotación, selector de pinceles     │
└─────────────────┬───────────────────────────────────────┬────────────────┘
                  ▼                                       ▼
        ┌──────────────────┐                    ┌──────────────────┐
        │ libmypaint (ext) │                    │ Qt 6 Widgets(ext)│
        └──────────────────┘                    └──────────────────┘
```

Las flechas indican dependencia y van siempre hacia abajo; en gris, las bibliotecas externas.

1. **Repositorio:** monorepo con `libs/` (bibliotecas compartidas) y `apps/` (una carpeta por aplicación), con CMake.
2. **UI:** Qt Widgets en todas las capas.
3. **Bibliotecas:** estáticas.
4. **`paintcore` (motor de pinceles):**
   1. Wrapper de libmypaint; ningún tipo de libmypaint se expone en su API, para poder cambiar de motor sin tocar las apps.
   2. Carga y gestión de pinceles `.myb`, con una ruta de pinceles compartida entre apps.
   3. Superficie de tiles sobre `QImage`.
   4. Adaptador de input: `QTabletEvent`/mouse → muestras de trazo, con la inversa de la transformación de vista.
   5. Widget de lienzo (`QWidget`) con rotación de vista, snap y reset.
   6. Widget selector de pinceles.
5. **`appkit` (framework común de app):**
   1. Ventana fullscreen con área útil configurable (rect mapeado a la tableta).
   2. Sistema de overlays: menú y panel de configuración.
   3. Configuración persistente (JSON) con secciones por app.
   4. Gestión de atajos de teclado.
6. **Cada app:** solo su lógica. En esta app: generadores de ejercicios con su geometría ideal, capa de guías, modo mixto y sus parámetros.
7. **Dependencias en un solo sentido:** app → `appkit` → `paintcore` → libmypaint. `paintcore` no conoce a `appkit`.

## Lienzo, área útil y rotación

El lienzo es fijo y a pantalla completa en el monitor ultra-wide, pero los ejercicios viven solo dentro del rectángulo mapeado a la tableta.

1. **Área útil configurable:** rectángulo que coincide con la porción del monitor mapeada a la tableta. Fuera de él, fondo oscuro o neutro.
2. **Zona segura:** los ejercicios se generan dentro del círculo inscripto en el área útil, para que nunca queden cortados al rotar.
3. **Rotación de vista:** tecla modificadora + arrastre con el lápiz, snap a 15° y una tecla para resetear.
4. **Rotación del ejercicio:** cada ejercicio se genera con orientación al azar.
5. **Sin zoom ni pan.**
6. **Puntos de fuga fuera de pantalla:** se indican por la dirección de las líneas guía.

## Interacción

Un intento por ejercicio, sin deshacer; todo se maneja con teclado y dos overlays.

1. **→** limpia el lienzo y genera el siguiente ejercicio.
2. **Repetir:** tecla para regenerar el mismo ejercicio (mismos parámetros y posición).
3. **Sin deshacer** dentro de un ejercicio.
4. **Menú overlay:** elegir tipo de ejercicio o el modo mixto.
5. **Modo mixto:** alterna tipos de ejercicio al azar, entre los que estén habilitados.
6. **Panel de configuración (overlay):** parámetros por ejercicio, pincel, área útil y colores.

## Ejercicios de la v1

La v1 incluye 15 ejercicios más el modo mixto: los 9 originales y 6 agregados (hatching, radiales, dirección forzada, elipses sobre eje, cajas rotadas y ghosting).

| # | Ejercicio | Grupo | Parámetros configurables |
| --- | --- | --- | --- |
| 1 | Dos puntos → recta | Líneas | Distancia mín/máx |
| 2 | Tres puntos → curva | Curvas | Distancia mín/máx, desvío mínimo de la recta |
| 3 | Hatching | Líneas | Tamaño del área, ángulo, espaciado sugerido |
| 4 | Radiales hacia un punto | Líneas | Cantidad de líneas, distancia al centro |
| 5 | Dirección forzada (con flecha) | Líneas | Distancia mín/máx, direcciones habilitadas |
| 6 | Elipses por grado | Elipses | Grados habilitados, tamaño mín/máx |
| 7 | Elipses concéntricas sobre un eje | Elipses | Cantidad, rango de grados |
| 8 | Elipses en perspectiva | Elipses / Perspectiva | Cantidad de PF, tamaño |
| 9 | Cajas en perspectiva | Perspectiva | 1, 2 o 3 PF; con o sin arista inicial |
| 10 | Cajas rotadas sobre su eje | Perspectiva | Cantidad de pasos de rotación |
| 11 | Escribir sobre una recta | Escritura | Largo, ángulo, interlineado |
| 12 | Escribir sobre una curva | Escritura | Curvatura máx |
| 13 | Escribir sobre varias curvas | Escritura | Cantidad de curvas, curvatura máx |
| 14 | Ghosting con memoria | Memoria | Tiempo visible, tipo de forma |
| 15 | Control de presión (perfil objetivo) | Presión | Perfiles habilitados: fino→grueso, constante, perfil libre |
| 16 | Modo mixto | — | Ejercicios habilitados en la rotación |

## Fuera de alcance (backlog)

Queda para después todo lo que requiere medir o recordar resultados.

1. **Evaluación de precisión:** desvío respecto de la geometría ideal, cierre de elipses, convergencia a los PF, ajuste al perfil de presión.
2. **Sesiones:** timer o contador de repeticiones por ejercicio.
3. **Historial y estadísticas de progreso.**
4. **Ejercicios restantes de la lista ampliada:** líneas superpuestas, copiar ángulo, dividir a ojo, curvas en C y S, espirales, ondas, tangentes, círculos inscriptos, óvalos Palmer, subdividir un plano, cilindros y conos, cross-contour, simetría, precisión de toque, escala de valor y trazos con taper.

## Notas técnicas

Tres decisiones de diseño dejan preparada la evaluación futura y evitan problemas con la rotación.

1. **Geometría ideal por ejercicio:** cada generador produce la forma objetivo (recta, curva, elipse, PF, perfil de presión) y la conserva aunque no se use. La evaluación se agrega después sin rediseñar.
2. **Coordenadas:** el trazo se pinta siempre en coordenadas del lienzo; la rotación es solo de la vista (`QTransform`) y al input se le aplica la inversa. Revisar el ángulo de vista que recibe libmypaint para pinceles sensibles a tilt o ascensión.
3. **Superficie MyPaint:** implementar una `MyPaintSurface` de tiles sobre un `QImage` del tamaño del área útil; limpiar = resetear la superficie.
4. **Capas de render:** guías del ejercicio y trazo del usuario en capas separadas, para poder ocultar las guías (ghosting) sin tocar el trazo.
