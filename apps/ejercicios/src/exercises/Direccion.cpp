#include "exercises/Direccion.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kDistMin = QStringLiteral("distMin");
const QString kDistMax = QStringLiteral("distMax");
const QString kJitter = QStringLiteral("jitter");

constexpr const char* kKeys[Direccion::kDirectionCount] = {"dirE", "dirSE", "dirS", "dirSW",
                                                           "dirW", "dirNW", "dirN", "dirNE"};
constexpr const char* kLabels[Direccion::kDirectionCount] = {
    "→  hacia la derecha", "↘  abajo a la derecha", "↓  hacia abajo",    "↙  abajo a la izquierda",
    "←  hacia la izquierda", "↖  arriba a la izquierda", "↑  hacia arriba", "↗  arriba a la derecha"};

constexpr qreal kArrowGap = 24; // la flecha va al costado, sin tapar la línea

qreal length(QPointF p)
{
    return QLineF(QPointF(), p).length();
}

} // namespace

QString Direccion::directionKey(int index)
{
    return QString::fromLatin1(kKeys[index]);
}

QString Direccion::id() const
{
    return QStringLiteral("direccion");
}

QString Direccion::title() const
{
    return QStringLiteral("Dirección forzada");
}

QString Direccion::group() const
{
    return QStringLiteral("Líneas");
}

QList<appkit::Param> Direccion::params() const
{
    using Type = appkit::Param::Type;
    QList<appkit::Param> list{
        {.key = kDistMin, .label = QStringLiteral("Largo mínimo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.25},
        {.key = kDistMax, .label = QStringLiteral("Largo máximo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.70},
    };
    for (int i = 0; i < kDirectionCount; ++i)
        list.append({.key = directionKey(i), .label = QString::fromUtf8(kLabels[i]), .type = Type::Toggle,
                     .defaultValue = true});
    list.append({.key = kJitter, .label = QStringLiteral("Variar ±15°"), .type = Type::Toggle, .defaultValue = false});
    return list;
}

Generated Direccion::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const qreal distMin = v.value(kDistMin).toDouble();
    const qreal distMax = qMax(distMin, v.value(kDistMax).toDouble());

    QList<int> enabled;
    for (int i = 0; i < kDirectionCount; ++i)
        if (v.value(directionKey(i)).toBool())
            enabled.append(i);
    if (enabled.isEmpty()) // sin ninguna, valen todas: el ejercicio nunca queda vacío
        for (int i = 0; i < kDirectionCount; ++i)
            enabled.append(i);
    const int direction = enabled[int(rng.bounded(quint32(enabled.size())))];
    const qreal jitter = (rng.generateDouble() * 2 - 1) * 15; // se sortea siempre
    const qreal degrees = directionDegrees(direction) + (v.value(kJitter).toBool() ? jitter : 0);
    const qreal theta = degrees * std::numbers::pi / 180;
    const QPointF u(std::cos(theta), std::sin(theta));
    const QPointF side = (rng.bounded(2) ? 1.0 : -1.0) * QPointF(-u.y(), u.x());

    // Extremos y flecha dentro de la zona: corrimiento al azar; si no aparece, se acorta.
    const qreal rTarget = zone.radius - appkit::theme::kRadioPuntoUnir - 2;
    const qreal rArrow = zone.radius - 8;
    qreal largo = qMin(2 * rTarget, (distMin + rng.generateDouble() * (distMax - distMin)) * 2 * zone.radius);
    QPointF a, b, from, to;
    for (bool placed = false; !placed;) {
        for (int attempt = 0; attempt < 60 && !placed; ++attempt) {
            const QPointF mid(rng.generateDouble() * 2 * zone.radius - zone.radius,
                              rng.generateDouble() * 2 * zone.radius - zone.radius);
            a = mid - u * largo / 2;
            b = mid + u * largo / 2;
            from = mid - u * largo / 4 + side * kArrowGap;
            to = mid + u * largo / 4 + side * kArrowGap;
            placed = length(a) <= rTarget && length(b) <= rTarget && length(from) <= rArrow && length(to) <= rArrow;
        }
        if (!placed)
            largo *= 0.9;
    }
    // Sobre la hoja, sin la orientación de la zona.
    a += zone.center;
    b += zone.center;
    from += zone.center;
    to += zone.center;

    Generated out;
    QPainterPath ideal(a);
    ideal.lineTo(b);
    out.ideal.append(ideal);
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::targetPoint(painter, a);
    appkit::guides::targetPoint(painter, b);
    appkit::guides::directionArrow(painter, from, to);
    return out;
}

} // namespace ejercicios
