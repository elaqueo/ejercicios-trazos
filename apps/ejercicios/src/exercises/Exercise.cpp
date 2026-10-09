#include "exercises/Exercise.h"

#include <QLineF>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

// Tolerancia para el borde: un punto generado justo sobre el círculo puede caer
// una fracción de píxel afuera por redondeo de coma flotante.
constexpr qreal kEpsilon = 1e-6;

} // namespace

SafeZone SafeZone::fromRect(const QRect& area, qreal orientation)
{
    SafeZone zone{QRectF(area).center(), qMin(area.width(), area.height()) / 2.0, orientation};
    zone.sheet = QRectF(area);
    return zone;
}

SafeZone SafeZone::withRandomOrientation(const QRect& area, quint32 seed)
{
    // Semilla distinta de la que usa el ejercicio: si no, el primer número al azar del
    // ejercicio coincidiría con la orientación.
    QRandomGenerator rng(~seed);
    return fromRect(area, rng.bounded(2 * std::numbers::pi));
}

QPointF SafeZone::pointAt(qreal angle, qreal dist) const
{
    const qreal a = angle + orientation;
    return center + QPointF(std::cos(a) * dist, std::sin(a) * dist);
}

QPointF SafeZone::toCanvas(QPointF local) const
{
    const qreal c = std::cos(orientation), s = std::sin(orientation);
    return center + QPointF(local.x() * c - local.y() * s, local.x() * s + local.y() * c);
}

bool SafeZone::contains(QPointF p) const
{
    return QLineF(center, p).length() <= radius + kEpsilon;
}

bool SafeZone::contains(const QPainterPath& path) const
{
    for (int i = 0; i < path.elementCount(); ++i) {
        if (!contains(QPointF(path.elementAt(i))))
            return false;
    }
    return true;
}

} // namespace ejercicios
