#pragma once

#include <QList>
#include <QPainterPath>
#include <QPointF>

namespace ejercicios {

// Línea base curva para escribir (HU-33, y la base de HU-34), en un marco local: la cuerda
// sobre x, de -length/2 a length/2, y "arriba de las letras" hacia +y. Dos formas:
//  - C: y = h · (1 − (2x/L)²), una sola panza; curvatura máxima 8h/L² en el medio.
//  - S: y = h · sen(2πx/L), cambia de lado; curvatura máxima 4π²h/L² en las panzas.
// En las dos la curvatura máxima está donde la pendiente es cero, así que acotarla ahí la
// acota en toda la curva.
enum class CurveShape { C, S };

struct WritingCurve {
    CurveShape shape = CurveShape::C;
    double length = 0;
    double height = 0; // h, con signo: hacia dónde va la panza

    double y(double x) const;
    double slope(double x) const;
    // La h más grande (en valor absoluto) con curvatura máxima ≤ 1/minRadius.
    static double maxHeight(CurveShape shape, double length, double minRadius);

    // `samples` + 1 puntos de la curva desplazada `offset` sobre la normal hacia arriba (0 =
    // la base), de izquierda a derecha, en el marco local.
    QList<QPointF> sample(double offset, int samples = 96) const;
};

// Curvatura de una polilínea en cada punto interior (por la circunferencia que pasa por él y
// sus dos vecinos); para los tests y para medir.
QList<double> polylineCurvature(const QList<QPointF>& points);

QPainterPath polylinePath(const QList<QPointF>& points);

} // namespace ejercicios
