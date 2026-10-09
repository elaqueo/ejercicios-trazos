#include "exercises/Recta.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QPainter>
#include <QRandomGenerator>

#include <cmath>

namespace ejercicios {

namespace {

const QString kDistMin = QStringLiteral("distMin");
const QString kDistMax = QStringLiteral("distMax");

qreal param(const QVariantMap& params, const QVariantMap& defaults, const QString& key)
{
    return params.value(key, defaults.value(key)).toDouble();
}

} // namespace

QString Recta::id() const
{
    return QStringLiteral("recta");
}

QString Recta::title() const
{
    return QStringLiteral("Dos puntos → recta");
}

QString Recta::group() const
{
    return QStringLiteral("Rectas");
}

QList<appkit::Param> Recta::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kDistMin, .label = QStringLiteral("Largo mínimo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.25},
        {.key = kDistMax, .label = QStringLiteral("Largo máximo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.80},
    };
}

Generated Recta::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap def = defaults();
    const qreal distMin = param(params, def, kDistMin);
    const qreal distMax = qMax(distMin, param(params, def, kDistMax));

    // El anillo de la guía también tiene que entrar en la zona.
    const qreal r = zone.radius - appkit::theme::kRadioPuntoUnir - 2;
    // Largo del segmento, como fracción del diámetro.
    const qreal largo = qMin(2 * r, (distMin + rng.bounded(distMax - distMin)) * 2 * zone.radius);
    const qreal mitad = largo / 2;

    // En el marco del ejercicio el segmento es horizontal (la orientación de la zona lo
    // rota) y su centro se corre al azar hasta donde los dos extremos sigan adentro:
    // primero la altura, después el corrimiento lateral que esa altura permite.
    const qreal alturaMax = std::sqrt(qMax(0.0, r * r - mitad * mitad));
    const qreal oy = rng.bounded(2 * alturaMax) - alturaMax;
    const qreal lateralMax = std::sqrt(qMax(0.0, r * r - oy * oy)) - mitad;
    const qreal ox = rng.bounded(2 * lateralMax) - lateralMax;

    const QPointF a = zone.toCanvas({ox - mitad, oy});
    const QPointF b = zone.toCanvas({ox + mitad, oy});

    Generated out;
    QPainterPath ideal(a);
    ideal.lineTo(b);
    out.ideal.append(ideal);
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::targetPoint(painter, a);
    appkit::guides::targetPoint(painter, b);
    return out;
}

} // namespace ejercicios
