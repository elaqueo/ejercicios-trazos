#include "exercises/WritingCurve.h"

#include <QLineF>

#include <cmath>
#include <numbers>

namespace ejercicios {

double WritingCurve::y(double x) const
{
    if (shape == CurveShape::C) {
        const double t = 2 * x / length;
        return height * (1 - t * t);
    }
    return height * std::sin(2 * std::numbers::pi * x / length);
}

double WritingCurve::slope(double x) const
{
    if (shape == CurveShape::C)
        return -8 * height * x / (length * length);
    const double k = 2 * std::numbers::pi / length;
    return height * k * std::cos(k * x);
}

double WritingCurve::maxHeight(CurveShape shape, double length, double minRadius)
{
    const double kappa = 1 / minRadius;
    return shape == CurveShape::C ? kappa * length * length / 8
                                  : kappa * length * length / (4 * std::numbers::pi * std::numbers::pi);
}

QList<QPointF> WritingCurve::sample(double offset, int samples) const
{
    QList<QPointF> points;
    points.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        const double x = -length / 2 + length * i / samples;
        const double m = slope(x);
        const double norm = std::sqrt(1 + m * m);
        points.append(QPointF(x, y(x)) + QPointF(-m, 1) * (offset / norm));
    }
    return points;
}

QList<double> polylineCurvature(const QList<QPointF>& points)
{
    QList<double> curvature;
    for (int i = 1; i + 1 < points.size(); ++i) {
        const QPointF a = points[i - 1], b = points[i], c = points[i + 1];
        const double area2 = std::abs((b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x()));
        const double sides = QLineF(a, b).length() * QLineF(b, c).length() * QLineF(a, c).length();
        curvature.append(sides > 0 ? 2 * area2 / sides : 0);
    }
    return curvature;
}

QPainterPath polylinePath(const QList<QPointF>& points)
{
    QPainterPath path;
    if (points.isEmpty())
        return path;
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i)
        path.lineTo(points[i]);
    return path;
}

} // namespace ejercicios
