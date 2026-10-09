#pragma once

#include "appkit/Theme.h"

#include <QColor>
#include <QPointF>

class QPainter;
class QPainterPath;

// Primitivas de guía con los estilos del diseño (docs/diseno.md, mesa "Guías"), para
// armar el dibujo que se pasa a lienzo::Lienzo::setGuides(). Son genéricas:
// cada app compone sus ejercicios con ellas. Coordenadas del lienzo.
namespace appkit::guides {

// Punto a unir: anillo de 24 px con centro (azul tinta).
// En ámbar (theme::kEnfasis), un destino al que convergen los trazos (radiales, HU-24).
void targetPoint(QPainter& painter, QPointF center, const QColor& color = theme::kGuia);

// Punto de paso: anillo chico y hueco, distinto de los extremos.
void passPoint(QPainter& painter, QPointF center);

// Línea guía firme (2 px, azul tinta): muestras, ejes, líneas base.
void guideLine(QPainter& painter, QPointF from, QPointF to);

// Línea de construcción (1,5 px punteada, azul suave): horizonte, líneas a PF.
void constructionLine(QPainter& painter, QPointF from, QPointF to);

// Las mismas, sobre un trazado cualquiera (líneas base curvas, HU-33).
void guidePath(QPainter& painter, const QPainterPath& path);
void constructionPath(QPainter& painter, const QPainterPath& path);

// Punto de fuga: rombo ámbar (HU-28).
void vanishingPoint(QPainter& painter, QPointF center);
// Punto de fuga fuera de la hoja: triángulo ámbar en el borde, apuntando hacia él (dir es la
// dirección hacia afuera, normalizada).
void offSheetVanishingPoint(QPainter& painter, QPointF edge, QPointF dir);

// Dirección del trazo: flecha ámbar con un punto en el arranque.
void directionArrow(QPainter& painter, QPointF from, QPointF to);

} // namespace appkit::guides
