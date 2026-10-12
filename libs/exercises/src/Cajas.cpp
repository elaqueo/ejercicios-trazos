#include "exercises/Cajas.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>
#include <optional>

namespace ejercicios {

namespace {

const QString kPf1 = QStringLiteral("pf1");
const QString kPf2 = QStringLiteral("pf2");
const QString kSeparation = QStringLiteral("separation");
const QString kPf3 = QStringLiteral("pf3");
const QString kThirdDistance = QStringLiteral("thirdDistance");
const QString kEdge = QStringLiteral("edge");
const QString kEdgeLength = QStringLiteral("edgeLength");

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
        {.key = kPf3, .label = QStringLiteral("3 puntos de fuga"), .type = Type::Toggle, .defaultValue = false},
        {.key = kSeparation, .label = QStringLiteral("Separación de los PF (× ancho de la hoja)"), .type = Type::Real,
         .minimum = 0.6, .maximum = 2.0, .step = 0.1, .defaultValue = 1.0, .decimals = 1},
        {.key = kThirdDistance, .label = QStringLiteral("Tercer PF: distancia al horizonte (× alto de la hoja)"),
         .type = Type::Real, .minimum = 1.0, .maximum = 4.0, .step = 0.25, .defaultValue = 2.0},
        {.key = kEdge, .label = QStringLiteral("Arista inicial"), .type = Type::Toggle, .defaultValue = false},
        {.key = kEdgeLength, .label = QStringLiteral("Largo de la arista (× alto de la hoja)"), .type = Type::Real,
         .minimum = 0.10, .maximum = 0.30, .step = 0.02, .defaultValue = 0.18},
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
    if (v.value(kPf3).toBool())
        modes.append(3);
    if (modes.isEmpty())
        modes = {1, 2, 3};
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

    // 3 PF: los dos del horizonte como con 2 PF, más el de las verticales (abajo).
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

    // El tercer PF (HU-29), sobre la vertical del foco y del lado de la caja: si la caja está
    // abajo del horizonte la vemos desde arriba y las verticales fugan hacia abajo.
    const qreal side = corner.y() > horizon ? 1.0 : -1.0;
    std::optional<QPointF> third;
    if (mode == 3) {
        third = QPointF(focus.x(), horizon + side * v.value(kThirdDistance).toDouble() * sheet.height());
        vanishing.append(*third);
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
        if (third && p == *third) {
            if (p.y() >= sheet.top() && p.y() <= sheet.bottom())
                continue;
            // Las del tercero: desde el horizonte, a cada lado del foco, hasta el borde.
            const qreal edgeY = p.y() < sheet.top() ? sheet.top() : sheet.bottom();
            for (const qreal dx : {-0.25, 0.25}) {
                const QPointF from(qBound(sheet.left() + 4, focus.x() + dx * sheet.width(), sheet.right() - 4),
                                   horizon + side * 4);
                const qreal t = (edgeY - from.y()) / (p.y() - from.y());
                examples.append(QLineF(from, from + t * (p - from)));
            }
            continue;
        }
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
    // La arista inicial (HU-29): la vertical más cercana de la caja, desde la esquina y
    // alejándose del horizonte; vertical exacta con 1 o 2 PF, hacia el tercer PF con 3.
    std::optional<QLineF> edge;
    if (v.value(kEdge).toBool()) {
        QPointF dir(0, side);
        if (third) {
            const QPointF d = *third - corner;
            dir = d / std::hypot(d.x(), d.y());
        }
        qreal length = v.value(kEdgeLength).toDouble() * sheet.height();
        // Que no se salga de la hoja.
        while (!sheet.adjusted(4, 4, -4, -4).contains(corner + dir * length) && length > 10)
            length *= 0.9;
        edge = QLineF(corner, corner + dir * length);
        QPainterPath path(edge->p1());
        path.lineTo(edge->p2());
        out.ideal.append(path);
    }

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionLine(painter, horizonPath.elementAt(0), horizonPath.elementAt(1));
    for (const QLineF& line : examples)
        appkit::guides::constructionLine(painter, line.p1(), line.p2());
    for (const QPointF& p : vanishing) {
        if (sheet.adjusted(10, 10, -10, -10).contains(p))
            appkit::guides::vanishingPoint(painter, p);
        else if (third && p == *third)
            appkit::guides::offSheetVanishingPoint(
                painter, QPointF(p.x(), p.y() < sheet.top() ? sheet.top() + 2 : sheet.bottom() - 2), QPointF(0, side));
        else if (p.x() < sheet.left() + 10)
            appkit::guides::offSheetVanishingPoint(painter, QPointF(sheet.left() + 2, horizon), QPointF(-1, 0));
        else
            appkit::guides::offSheetVanishingPoint(painter, QPointF(sheet.right() - 2, horizon), QPointF(1, 0));
    }
    if (edge)
        appkit::guides::guideLine(painter, edge->p1(), edge->p2());
    appkit::guides::targetPoint(painter, corner);
    return out;
}

} // namespace ejercicios
