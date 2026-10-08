#include "exercises/Exercise.h"

#include <QLineF>

#include <cmath>

namespace ejercicios {

SafeZone SafeZone::fromRect(const QRect& area)
{
    return {QRectF(area).center(), qMin(area.width(), area.height()) / 2.0};
}

QPointF SafeZone::pointAt(qreal angle, qreal dist) const
{
    return center + QPointF(std::cos(angle) * dist, std::sin(angle) * dist);
}

bool SafeZone::contains(QPointF p) const
{
    return QLineF(center, p).length() <= radius;
}

} // namespace ejercicios
