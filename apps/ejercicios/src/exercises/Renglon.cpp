#include "exercises/Renglon.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QPainter>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kLength = QStringLiteral("length");
const QString kSpacing = QStringLiteral("spacing");
const QString kXHeight = QStringLiteral("xHeight");
const QString kRandomAngle = QStringLiteral("randomAngle");
const QString kAngle = QStringLiteral("angle");

QPainterPath segment(QPointF from, QPointF to)
{
    QPainterPath path(from);
    path.lineTo(to);
    return path;
}

} // namespace

QString Renglon::id() const
{
    return QStringLiteral("renglon");
}

QString Renglon::title() const
{
    return QStringLiteral("Escribir sobre una recta");
}

QString Renglon::group() const
{
    return QStringLiteral("Escritura");
}

QList<appkit::Param> Renglon::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kLength, .label = QStringLiteral("Largo (× diámetro de la zona)"), .type = Type::Real, .minimum = 0.4,
         .maximum = 0.9, .step = 0.05, .defaultValue = 0.70},
        {.key = kSpacing, .label = QStringLiteral("Interlineado"), .type = Type::Real, .minimum = 6, .maximum = 25,
         .step = 0.5, .defaultValue = 12.0, .decimals = 1, .suffix = QStringLiteral(" mm")},
        {.key = kXHeight, .label = QStringLiteral("Altura de x (× interlineado)"), .type = Type::Real, .minimum = 0.3,
         .maximum = 0.6, .step = 0.05, .defaultValue = 0.40},
        {.key = kRandomAngle, .label = QStringLiteral("Ángulo al azar (±20°)"), .type = Type::Toggle,
         .defaultValue = false},
        {.key = kAngle, .label = QStringLiteral("Ángulo fijo"), .type = Type::Integer, .minimum = -30, .maximum = 30,
         .step = 1, .defaultValue = 0, .suffix = QStringLiteral("°")},
    };
}

Generated Renglon::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const qreal randomDegrees = rng.generateDouble() * 40 - 20; // se sortea siempre
    const qreal degrees = v.value(kRandomAngle).toBool() ? randomDegrees : v.value(kAngle).toDouble();
    const qreal theta = degrees * std::numbers::pi / 180;
    // Sobre la hoja (y hacia abajo): u avanza el texto, n apunta hacia arriba de las letras.
    const QPointF u(std::cos(theta), -std::sin(theta));
    const QPointF n(-std::sin(theta), -std::cos(theta));

    const qreal spacing = zone.mm(v.value(kSpacing).toDouble());
    const qreal xHeight = v.value(kXHeight).toDouble() * spacing;
    qreal length = v.value(kLength).toDouble() * 2 * zone.radius;
    // El bloque del renglón (largo × interlineado) entra en la zona: se acorta si hace falta y
    // se corre al azar lo que sobra.
    const qreal margin = 8;
    while (std::hypot(length / 2, spacing / 2) > zone.radius - margin && length > 1)
        length *= 0.95;
    const qreal room = std::max(0.0, zone.radius - margin - std::hypot(length / 2, spacing / 2));
    const qreal along = rng.generateDouble() * 2 * std::numbers::pi;
    const QPointF blockCenter =
        zone.center + QPointF(std::cos(along), std::sin(along)) * room * std::sqrt(rng.generateDouble());
    const QPointF start = blockCenter - u * length / 2 - n * spacing / 2;
    const QPointF end = start + u * length;

    Generated out;
    out.ideal.append(segment(start, end));
    out.ideal.append(segment(start + n * xHeight, end + n * xHeight));
    out.ideal.append(segment(start + n * spacing, end + n * spacing));

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionLine(painter, start + n * spacing, end + n * spacing);
    appkit::guides::constructionLine(painter, start + n * xHeight, end + n * xHeight);
    appkit::guides::guideLine(painter, start, end);
    painter.setPen(Qt::NoPen);
    painter.setBrush(appkit::theme::kGuia);
    painter.drawEllipse(start, 4.5, 4.5); // dónde empezar
    return out;
}

} // namespace ejercicios
