#include "paintcore/ViewTransform.h"

#include <cmath>

namespace paintcore {

void ViewTransform::setCenter(QPointF center)
{
    m_center = center;
    update();
}

void ViewTransform::setAngle(double degrees)
{
    m_angle = normalized(degrees);
    update();
}

QPointF ViewTransform::toView(QPointF canvasPoint) const
{
    return m_matrix.map(canvasPoint);
}

QPointF ViewTransform::toCanvas(QPointF viewPoint) const
{
    return m_inverse.map(viewPoint);
}

double ViewTransform::normalized(double degrees)
{
    const double result = std::fmod(degrees, 360.0);
    return result < 0.0 ? result + 360.0 : (result >= 360.0 ? 0.0 : result);
}

double ViewTransform::snapped(double degrees, double step)
{
    return normalized(std::round(degrees / step) * step);
}

void ViewTransform::update()
{
    m_matrix = QTransform::fromTranslate(m_center.x(), m_center.y());
    m_matrix.rotate(m_angle);
    m_matrix.translate(-m_center.x(), -m_center.y());
    m_inverse = m_matrix.inverted();
}

} // namespace paintcore
