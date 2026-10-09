#include "exercises/Radiales.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QPainter>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kCount = QStringLiteral("count");
const QString kDist = QStringLiteral("dist");
const QString kSector = QStringLiteral("sector");

} // namespace

QString Radiales::id() const
{
    return QStringLiteral("radiales");
}

QString Radiales::title() const
{
    return QStringLiteral("Radiales hacia un punto");
}

QString Radiales::group() const
{
    return QStringLiteral("Líneas");
}

QList<appkit::Param> Radiales::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kCount, .label = QStringLiteral("Cantidad de líneas"), .type = Type::Integer, .minimum = 3,
         .maximum = 16, .step = 1, .defaultValue = 8},
        {.key = kDist, .label = QStringLiteral("Distancia al centro (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.15, .maximum = 0.45, .step = 0.05, .defaultValue = 0.35},
        {.key = kSector, .label = QStringLiteral("Solo un sector (180° o 270°)"), .type = Type::Toggle,
         .defaultValue = false},
    };
}

Generated Radiales::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const int count = v.value(kCount).toInt();
    const bool sector = v.value(kSector).toBool();

    // Los anillos de partida tienen que entrar en la zona: el centro se corre al azar lo que
    // la distancia deja libre.
    const qreal rTarget = zone.radius - appkit::theme::kRadioPuntoUnir - 2;
    const qreal dist = std::min(v.value(kDist).toDouble() * 2 * zone.radius, rTarget);
    const qreal room = rTarget - dist;
    const qreal along = rng.generateDouble() * 2 * std::numbers::pi;
    const QPointF center = QPointF(std::cos(along), std::sin(along)) * room * std::sqrt(rng.generateDouble());

    // Ángulos: parejos en la vuelta entera, o de punta a punta del sector; cada uno con ±20 %
    // del paso (en un sector, los extremos solo hacia adentro, para no pasarse del arco).
    const qreal span = (rng.bounded(2) ? 270.0 : 180.0) * std::numbers::pi / 180;
    const qreal start = rng.generateDouble() * 2 * std::numbers::pi;
    const qreal step = sector ? span / (count - 1) : 2 * std::numbers::pi / count;
    QList<QPointF> starts;
    for (int i = 0; i < count; ++i) {
        qreal jitter = (rng.generateDouble() * 2 - 1) * 0.2 * step;
        if (sector && i == 0)
            jitter = std::abs(jitter);
        if (sector && i == count - 1)
            jitter = -std::abs(jitter);
        const qreal angle = start + i * step + jitter;
        starts.append(center + QPointF(std::cos(angle), std::sin(angle)) * dist);
    }

    const QPointF target = zone.toCanvas(center);
    Generated out;
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    for (const QPointF& local : starts) {
        const QPointF from = zone.toCanvas(local);
        QPainterPath path(from);
        path.lineTo(target);
        out.ideal.append(path);
        appkit::guides::targetPoint(painter, from);
    }
    appkit::guides::targetPoint(painter, target, appkit::theme::kEnfasis);
    return out;
}

} // namespace ejercicios
