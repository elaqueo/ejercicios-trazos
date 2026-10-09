#include "appkit/Guides.h"

#include "appkit/Theme.h"

#include <QLineF>
#include <QPainter>
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

void constructionLine(QPainter& painter, QPointF from, QPointF to)
{
    painter.save();
    QPen pen(theme::kGuiaSuave, theme::kGrosorConstruccion);
    pen.setDashPattern({4.0 / theme::kGrosorConstruccion, 5.0 / theme::kGrosorConstruccion}); // 4 px / 5 px
    painter.setPen(pen);
    painter.drawLine(from, to);
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
