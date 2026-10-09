#include "exercises/Cajas.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kPf1 = QStringLiteral("pf1");
const QString kPf2 = QStringLiteral("pf2");
const QString kSeparation = QStringLiteral("separation");

constexpr qreal kMargin = 60; // la esquina, lejos del borde de la hoja

QPainterPath point(QPointF p)
{
    QPainterPath path;
    path.moveTo(p);
    return path;
}

} // namespace

QString Cajas::id() const
{
    return QStringLiteral("cajas");
}

QString Cajas::title() const
{
    return QStringLiteral("Cajas en perspectiva");
}

QString Cajas::group() const
{
    return QStringLiteral("Perspectiva");
}

QList<appkit::Param> Cajas::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kPf1, .label = QStringLiteral("1 punto de fuga"), .type = Type::Toggle, .defaultValue = true},
        {.key = kPf2, .label = QStringLiteral("2 puntos de fuga"), .type = Type::Toggle, .defaultValue = true},
        {.key = kSeparation, .label = QStringLiteral("Separación de los PF (× ancho de la hoja)"), .type = Type::Real,
         .minimum = 0.6, .maximum = 2.0, .step = 0.1, .defaultValue = 1.0, .decimals = 1},
    };
}

Generated Cajas::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    QList<int> modes;
    if (v.value(kPf1).toBool())
        modes.append(1);
    if (v.value(kPf2).toBool())
        modes.append(2);
    if (modes.isEmpty())
        modes = {1, 2};
    const int mode = modes[int(rng.bounded(quint32(modes.size())))];

    const QRectF sheet = zone.sheet.isValid() ? zone.sheet
                                              : QRectF(zone.center - QPointF(zone.radius, zone.radius),
                                                       QSizeF(2 * zone.radius, 2 * zone.radius));
    const QRectF inner = sheet.adjusted(kMargin, kMargin, -kMargin, -kMargin);
    const qreal horizon = sheet.top() + sheet.height() * (0.3 + 0.4 * rng.generateDouble());

    // Los PF y el centro del cono de visión.
    QList<QPointF> vanishing;
    QPointF focus;
    qreal coneRadius = 0;
    if (mode == 1) {
        focus = QPointF(sheet.left() + sheet.width() * (0.3 + 0.4 * rng.generateDouble()), horizon);
        vanishing = {focus};
        coneRadius = 0.35 * sheet.height();
    } else {
        const qreal separation = v.value(kSeparation).toDouble() * sheet.width();
        focus = QPointF(sheet.center().x() + (rng.generateDouble() - 0.5) * 0.2 * sheet.width(), horizon);
        vanishing = {focus - QPointF(separation / 2, 0), focus + QPointF(separation / 2, 0)};
        coneRadius = separation / 3;
    }

    // La esquina: dentro del cono, fuera del horizonte (la caja se ve de arriba o de abajo) y
    // dentro de la hoja. Si el azar no la encuentra, va arriba o abajo del foco.
    const qreal minLift = 0.08 * sheet.height();
    QPointF corner;
    bool placed = false;
    for (int attempt = 0; attempt < 200 && !placed; ++attempt) {
        const qreal angle = rng.generateDouble() * 2 * std::numbers::pi;
        const qreal r = coneRadius * std::sqrt(0.09 + 0.91 * rng.generateDouble());
        corner = focus + QPointF(std::cos(angle), std::sin(angle)) * r;
        placed = inner.contains(corner) && std::abs(corner.y() - horizon) >= minLift;
    }
    if (!placed) {
        const qreal down = horizon + std::min(coneRadius, inner.bottom() - horizon);
        const qreal up = horizon - std::min(coneRadius, horizon - inner.top());
        corner = QPointF(qBound(inner.left(), focus.x(), inner.right()), down - horizon >= horizon - up ? down : up);
    }

    Generated out;
    QPainterPath horizonPath(QPointF(sheet.left(), horizon));
    horizonPath.lineTo(QPointF(sheet.right(), horizon));
    out.ideal.append(horizonPath);
    out.ideal.append(point(corner));
    for (const QPointF& p : vanishing)
        out.ideal.append(point(p));
    // Un PF fuera de la hoja no se ve: dos rectas de ejemplo que fugan hacia él, una por
    // encima y otra por debajo del horizonte, desde la zona de la caja hasta el borde (pedido
    // del usuario).
    QList<QLineF> examples;
    const qreal lift = 0.3 * sheet.height();
    for (const QPointF& p : vanishing) {
        if (p.x() >= sheet.left() && p.x() <= sheet.right())
            continue;
        const qreal edgeX = p.x() < sheet.left() ? sheet.left() : sheet.right();
        for (const qreal dy : {-lift, lift}) {
            const QPointF from(focus.x(), qBound(sheet.top() + 4, horizon + dy, sheet.bottom() - 4));
            const qreal t = (edgeX - from.x()) / (p.x() - from.x());
            examples.append(QLineF(from, from + t * (p - from)));
        }
    }
    for (const QLineF& line : examples) {
        QPainterPath path(line.p1());
        path.lineTo(line.p2());
        out.ideal.append(path);
    }

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionLine(painter, horizonPath.elementAt(0), horizonPath.elementAt(1));
    for (const QLineF& line : examples)
        appkit::guides::constructionLine(painter, line.p1(), line.p2());
    for (const QPointF& p : vanishing) {
        if (sheet.adjusted(10, 0, -10, 0).contains(QPointF(p.x(), sheet.center().y())))
            appkit::guides::vanishingPoint(painter, p);
        else if (p.x() < sheet.left() + 10)
            appkit::guides::offSheetVanishingPoint(painter, QPointF(sheet.left() + 2, horizon), QPointF(-1, 0));
        else
            appkit::guides::offSheetVanishingPoint(painter, QPointF(sheet.right() - 2, horizon), QPointF(1, 0));
    }
    appkit::guides::targetPoint(painter, corner);
    return out;
}

} // namespace ejercicios
