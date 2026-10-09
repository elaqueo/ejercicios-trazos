#include "appkit/Guides.h"

#include "appkit/Theme.h"

#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace appkit::guides {

void targetPoint(QPainter& painter, QPointF center, const QColor& color)
{
    painter.save();
    painter.setPen(QPen(color, theme::kGrosorGuia));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, theme::kRadioPuntoUnir, theme::kRadioPuntoUnir);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(center, 4.5, 4.5);
    painter.restore();
}

void passPoint(QPainter& painter, QPointF center)
{
    painter.save();
    painter.setPen(QPen(theme::kGuia, theme::kGrosorGuia));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, theme::kRadioPuntoPaso, theme::kRadioPuntoPaso);
    painter.restore();
}

void guideLine(QPainter& painter, QPointF from, QPointF to)
{
    painter.save();
    painter.setPen(QPen(theme::kGuia, theme::kGrosorGuia, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(from, to);
    painter.restore();
}

void guidePath(QPainter& painter, const QPainterPath& path)
{
    painter.save();
    painter.setPen(QPen(theme::kGuia, theme::kGrosorGuia, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.restore();
}

void constructionPath(QPainter& painter, const QPainterPath& path)
{
    painter.save();
    QPen pen(theme::kGuiaSuave, theme::kGrosorConstruccion);
    pen.setDashPattern({4.0 / theme::kGrosorConstruccion, 5.0 / theme::kGrosorConstruccion});
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.restore();
}

void constructionLine(QPainter& painter, QPointF from, QPointF to)
{
    painter.save();
    QPen pen(theme::kGuiaSuave, theme::kGrosorConstruccion);
    pen.setDashPattern({4.0 / theme::kGrosorConstruccion, 5.0 / theme::kGrosorConstruccion}); // 4 px / 5 px
    painter.setPen(pen);
    painter.drawLine(from, to);
    painter.restore();
}

void vanishingPoint(QPainter& painter, QPointF center)
{
    constexpr double kHalf = 9.0;
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::kEnfasis);
    const QPointF diamond[4] = {center + QPointF(0, -kHalf), center + QPointF(kHalf, 0), center + QPointF(0, kHalf),
                                center + QPointF(-kHalf, 0)};
    painter.drawPolygon(diamond, 4);
    painter.restore();
}

void offSheetVanishingPoint(QPainter& painter, QPointF edge, QPointF dir)
{
    constexpr double kLength = 22.0, kHalfWidth = 10.0;
    const QPointF side(-dir.y(), dir.x());
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::kEnfasis);
    const QPointF tip = edge;
    const QPointF base = edge - dir * kLength;
    const QPointF triangle[3] = {tip, base + side * kHalfWidth, base - side * kHalfWidth};
    painter.drawPolygon(triangle, 3);
    painter.restore();
}

void directionArrow(QPainter& painter, QPointF from, QPointF to)
{
    constexpr double kWidth = 3.0;
    constexpr double kHead = 22.0;
    painter.save();
    painter.setPen(QPen(theme::kEnfasis, kWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(from, to);
    // Punta: dos trazos a ±30° del sentido, hacia atrás.
    const QLineF back(to, from);
    for (const double angle : {30.0, -30.0}) {
        QLineF wing = QLineF::fromPolar(kHead, back.angle() + angle);
        wing.translate(to);
        painter.drawLine(wing);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::kEnfasis);
    painter.drawEllipse(from, 5.0, 5.0);
    painter.restore();
}

} // namespace appkit::guides
