#include "exercises/CajasRotadas.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QFont>
#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kCount = QStringLiteral("count");
const QString kStep = QStringLiteral("step");
const QString kEye = QStringLiteral("eye");

constexpr qreal kMaxAngle = 85; // más cerca de 90°, un PF se va al infinito

QPainterPath point(QPointF p)
{
    QPainterPath path;
    path.moveTo(p);
    return path;
}

void label(QPainter& painter, QPointF center, const QString& text, const QColor& color)
{
    painter.save();
    QFont font(QString::fromLatin1(appkit::theme::kFuenteMono));
    font.setPixelSize(18);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(QRectF(center.x() - 60, center.y() - 13, 120, 26), Qt::AlignCenter, text);
    painter.restore();
}

} // namespace

QString CajasRotadas::id() const
{
    return QStringLiteral("cajasRotadas");
}

QString CajasRotadas::title() const
{
    return QStringLiteral("Cajas rotadas");
}

QString CajasRotadas::group() const
{
    return QStringLiteral("Perspectiva");
}

QList<appkit::Param> CajasRotadas::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kCount, .label = QStringLiteral("Cantidad de cajas"), .type = Type::Integer, .minimum = 3,
         .maximum = 5, .step = 1, .defaultValue = 4},
        {.key = kStep, .label = QStringLiteral("Paso de giro"), .type = Type::Integer, .minimum = 10, .maximum = 30,
         .step = 5, .defaultValue = 15, .suffix = QStringLiteral("°")},
        {.key = kEye, .label = QStringLiteral("Distancia del ojo (× ancho de la hoja)"), .type = Type::Real,
         .minimum = 0.3, .maximum = 1.0, .step = 0.05, .defaultValue = 0.5},
    };
}

Generated CajasRotadas::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const int count = v.value(kCount).toInt();
    // El giro total no pasa de 80°: si el paso no entra, se achica a múltiplos de 5°.
    int step = v.value(kStep).toInt();
    while ((count - 1) * step > 80 && step > 5)
        step -= 5;
    const int span = (count - 1) * step;
    const int startSteps = int(rng.bounded(quint32((int(kMaxAngle) - span - 5) / 5 + 1))); // 5° … 85° − giro
    const int start = 5 + 5 * startSteps;

    const QRectF sheet = zone.sheet.isValid() ? zone.sheet
                                              : QRectF(zone.center - QPointF(zone.radius, zone.radius),
                                                       QSizeF(2 * zone.radius, 2 * zone.radius));
    const qreal side = rng.bounded(2) ? 1.0 : -1.0; // 1: las cajas abajo del horizonte
    const qreal horizon = sheet.top() + sheet.height() * (side > 0 ? 0.35 + 0.1 * rng.generateDouble()
                                                                   : 0.55 + 0.1 * rng.generateDouble());
    const qreal cx = sheet.center().x(); // centro de visión
    const qreal f = v.value(kEye).toDouble() * sheet.width();
    const qreal lift = 0.25 * sheet.height();
    const qreal rayLength = 0.09 * sheet.width();

    Generated out;
    QPainterPath horizonPath(QPointF(sheet.left(), horizon));
    horizonPath.lineTo(QPointF(sheet.right(), horizon));
    out.ideal.append(horizonPath);

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionLine(painter, horizonPath.elementAt(0), horizonPath.elementAt(1));
    int offLeft = 0, offRight = 0; // triángulos ya puestos en cada borde
    for (int k = 0; k < count; ++k) {
        const int degrees = start + k * step;
        const qreal alpha = degrees * std::numbers::pi / 180;
        const QPointF vpA(cx + f * std::tan(alpha), horizon);
        const QPointF vpB(cx - f / std::tan(alpha), horizon);
        const QPointF corner(sheet.left() + sheet.width() * (k + 1) / (count + 1), horizon + side * lift);
        out.ideal.append(point(corner));
        out.ideal.append(point(vpA));
        out.ideal.append(point(vpB));

        // Las aristas de la base, cortas, hacia los dos PF.
        for (const QPointF& vp : {vpA, vpB}) {
            const QPointF d = vp - corner;
            appkit::guides::constructionLine(painter, corner, corner + d / std::hypot(d.x(), d.y()) * rayLength);
        }
        appkit::guides::targetPoint(painter, corner);
        label(painter, corner + QPointF(0, side * 30), QStringLiteral("%1 · %2°").arg(k + 1).arg(degrees),
              appkit::theme::kGuia);

        const QString number = QString::number(k + 1);
        for (const QPointF& vp : {vpA, vpB}) {
            if (vp.x() > sheet.left() + 10 && vp.x() < sheet.right() - 10) {
                appkit::guides::vanishingPoint(painter, vp);
                label(painter, vp - QPointF(0, side * 22), number, appkit::theme::kEnfasis);
            } else {
                // Afuera: triángulo en el borde, apilados del lado opuesto a las cajas.
                const bool left = vp.x() <= sheet.left() + 10;
                int& stacked = left ? offLeft : offRight;
                const QPointF edge(left ? sheet.left() + 2 : sheet.right() - 2, horizon - side * 26 * stacked);
                ++stacked;
                appkit::guides::offSheetVanishingPoint(painter, edge, QPointF(left ? -1 : 1, 0));
                label(painter, edge + QPointF(left ? 40 : -40, 0), number, appkit::theme::kEnfasis);
            }
        }
    }
    return out;
}

} // namespace ejercicios
