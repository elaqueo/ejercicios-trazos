#include "exercises/CurvaDireccion.h"

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
const QString kDevMin = QStringLiteral("devMin");
const QString kDevMax = QStringLiteral("devMax");
const QString kJitter = QStringLiteral("jitter");

constexpr qreal kArrowGap = 24; // como en Direccion: al costado, sin tapar la curva

qreal length(QPointF p)
{
    return QLineF(QPointF(), p).length();
}

} // namespace

QString CurvaDireccion::id() const
{
    return QStringLiteral("curvaDireccion");
}

QString CurvaDireccion::title() const
{
    return QStringLiteral("Dirección forzada");
}

QString CurvaDireccion::group() const
{
    return QStringLiteral("Curvas");
}

QList<appkit::Param> CurvaDireccion::params() const
{
    using Type = appkit::Param::Type;
    QList<appkit::Param> list{
        {.key = kDistMin, .label = QStringLiteral("Largo mínimo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.30},
        {.key = kDistMax, .label = QStringLiteral("Largo máximo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.75},
        {.key = kDevMin, .label = QStringLiteral("Desvío mínimo (× largo)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 0.5, .step = 0.05, .defaultValue = 0.10},
        {.key = kDevMax, .label = QStringLiteral("Desvío máximo (× largo)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 0.5, .step = 0.05, .defaultValue = 0.35},
    };
    for (int i = 0; i < Direccion::kDirectionCount; ++i)
        list.append({.key = Direccion::directionKey(i), .label = Direccion::directionLabel(i), .type = Type::Toggle,
                     .defaultValue = true});
    list.append({.key = kJitter, .label = QStringLiteral("Variar ±15°"), .type = Type::Toggle, .defaultValue = false});
    return list;
}

Generated CurvaDireccion::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const qreal distMin = v.value(kDistMin).toDouble();
    const qreal distMax = qMax(distMin, v.value(kDistMax).toDouble());
    const qreal devMin = v.value(kDevMin).toDouble();
    const qreal devMax = qMax(devMin, v.value(kDevMax).toDouble());

    QList<int> enabled;
    for (int i = 0; i < Direccion::kDirectionCount; ++i)
        if (v.value(Direccion::directionKey(i)).toBool())
            enabled.append(i);
    if (enabled.isEmpty()) // sin ninguna, valen todas: el ejercicio nunca queda vacío
        for (int i = 0; i < Direccion::kDirectionCount; ++i)
            enabled.append(i);
    const int direction = enabled[int(rng.bounded(quint32(enabled.size())))];
    const qreal jitter = (rng.generateDouble() * 2 - 1) * 15; // se sortea siempre
    const qreal degrees = Direccion::directionDegrees(direction) + (v.value(kJitter).toBool() ? jitter : 0);
    const qreal theta = degrees * std::numbers::pi / 180;
    const QPointF u(std::cos(theta), std::sin(theta));
    const QPointF bulge = (rng.bounded(2) ? 1.0 : -1.0) * QPointF(-u.y(), u.x()); // hacia la panza
    const qreal desvio = devMin + rng.generateDouble() * (devMax - devMin);

    // Extremos, control de la cuadrática (al doble del desvío, como Curva) y flecha (del lado
    // de afuera) dentro de la zona: corrimiento al azar; si no aparece, se achica la figura.
    const qreal rTarget = zone.radius - appkit::theme::kRadioPuntoUnir - 2;
    const qreal rArrow = zone.radius - 8;
    qreal largo = qMin(2 * rTarget, (distMin + rng.generateDouble() * (distMax - distMin)) * 2 * zone.radius);
    QPointF a, b, control, from, to;
    for (bool placed = false; !placed;) {
        const qreal h = desvio * largo;
        for (int attempt = 0; attempt < 60 && !placed; ++attempt) {
            const QPointF mid(rng.generateDouble() * 2 * zone.radius - zone.radius,
                              rng.generateDouble() * 2 * zone.radius - zone.radius);
            a = mid - u * largo / 2;
            b = mid + u * largo / 2;
            control = mid + bulge * 2 * h;
            from = mid - u * largo / 4 - bulge * kArrowGap;
            to = mid + u * largo / 4 - bulge * kArrowGap;
            placed = length(a) <= rTarget && length(b) <= rTarget && length(control) <= zone.radius
                     && length(from) <= rArrow && length(to) <= rArrow;
        }
        if (!placed)
            largo *= 0.9;
    }
    // Sobre la hoja, sin la orientación de la zona.
    a += zone.center;
    b += zone.center;
    control += zone.center;
    from += zone.center;
    to += zone.center;

    Generated out;
    QPainterPath ideal(a);
    ideal.quadTo(control, b);
    out.ideal.append(ideal);
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionPath(painter, ideal); // la curva a lograr, tenue
    appkit::guides::targetPoint(painter, a);
    appkit::guides::targetPoint(painter, b);
    appkit::guides::directionArrow(painter, from, to);
    return out;
}

} // namespace ejercicios
