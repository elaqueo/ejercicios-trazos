#pragma once

#include <QPointF>

class QPainter;

// Primitivas de guía con los estilos del diseño (docs/diseno.md, mesa "Guías"), para
// armar el dibujo que se pasa a lienzo::Lienzo::setGuides(). Son genéricas:
// cada app compone sus ejercicios con ellas. Coordenadas del lienzo.
namespace appkit::guides {

// Punto a unir: anillo de 24 px con centro (azul tinta).
void targetPoint(QPainter& painter, QPointF center);

// Punto de paso: anillo chico y hueco, distinto de los extremos.
void passPoint(QPainter& painter, QPointF center);

// Línea guía firme (2 px, azul tinta): muestras, ejes, líneas base.
void guideLine(QPainter& painter, QPointF from, QPointF to);

// Línea de construcción (1,5 px punteada, azul suave): horizonte, líneas a PF.
void constructionLine(QPainter& painter, QPointF from, QPointF to);

// Dirección del trazo: flecha ámbar con un punto en el arranque.
void directionArrow(QPainter& painter, QPointF from, QPointF to);

} // namespace appkit::guides
