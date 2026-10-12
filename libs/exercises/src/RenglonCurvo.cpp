#include "exercises/RenglonCurvo.h"

#include "exercises/WritingCurve.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kLength = QStringLiteral("length");
const QString kSpacing = QStringLiteral("spacing");
const QString kXHeight = QStringLiteral("xHeight");
const QString kMinRadius = QStringLiteral("minRadius");
const QString kAllowS = QStringLiteral("allowS");
const QString kRandomAngle = QStringLiteral("randomAngle");
const QString kAngle = QStringLiteral("angle");

} // namespace

QString RenglonCurvo::id() const
{
    return QStringLiteral("renglonCurvo");
}

QString RenglonCurvo::title() const
{
    return QStringLiteral("Escribir sobre una curva");
}

QString RenglonCurvo::group() const
{
    return QStringLiteral("Escritura");
}

QList<appkit::Param> RenglonCurvo::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kLength, .label = QStringLiteral("Largo (× diámetro de la zona)"), .type = Type::Real, .minimum = 0.4,
         .maximum = 0.9, .step = 0.05, .defaultValue = 0.70},
        {.key = kSpacing, .label = QStringLiteral("Interlineado"), .type = Type::Real, .minimum = 6, .maximum = 25,
         .step = 0.5, .defaultValue = 12.0, .decimals = 1, .suffix = QStringLiteral(" mm")},
        {.key = kXHeight, .label = QStringLiteral("Altura de x (× interlineado)"), .type = Type::Real, .minimum = 0.3,
         .maximum = 0.6, .step = 0.05, .defaultValue = 0.40},
        {.key = kMinRadius, .label = QStringLiteral("Radio mínimo de la curva"), .type = Type::Integer, .minimum = 40,
         .maximum = 300, .step = 10, .defaultValue = 80, .suffix = QStringLiteral(" mm")},
        {.key = kAllowS, .label = QStringLiteral("Permitir S"), .type = Type::Toggle, .defaultValue = true},
        {.key = kRandomAngle, .label = QStringLiteral("Ángulo al azar (±20°)"), .type = Type::Toggle,
         .defaultValue = false},
        {.key = kAngle, .label = QStringLiteral("Ángulo fijo"), .type = Type::Integer, .minimum = -30, .maximum = 30,
         .step = 1, .defaultValue = 0, .suffix = QStringLiteral("°")},
    };
}

Generated RenglonCurvo::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    // Todo se sortea siempre, en el mismo orden: misma semilla, mismo resto.
    const qreal randomDegrees = rng.generateDouble() * 40 - 20;
    const bool sShape = rng.bounded(2) == 1;
    const qreal bend = 0.4 + 0.6 * rng.generateDouble(); // fracción de la panza máxima
    const qreal side = rng.bounded(2) ? 1.0 : -1.0;

    const qreal degrees = v.value(kRandomAngle).toBool() ? randomDegrees : v.value(kAngle).toDouble();
    const qreal theta = degrees * std::numbers::pi / 180;
    const QPointF u(std::cos(theta), -std::sin(theta));  // avanza el texto
    const QPointF n(-std::sin(theta), -std::cos(theta)); // arriba de las letras

    const qreal spacing = zone.mm(v.value(kSpacing).toDouble());
    const qreal xHeight = v.value(kXHeight).toDouble() * spacing;
    const qreal minRadius = std::max(zone.mm(v.value(kMinRadius).toDouble()), 1.5 * spacing);

    WritingCurve curve;
    curve.shape = sShape && v.value(kAllowS).toBool() ? CurveShape::S : CurveShape::C;
    curve.length = v.value(kLength).toDouble() * 2 * zone.radius;
    QList<QPointF> base, xLine, top;
    QPointF blockCenter;
    qreal reach = 0;
    const qreal margin = 8;
    for (;;) {
        // La panza: una fracción de la máxima que permite el radio, y no más de un cuarto del
        // largo (C) o un octavo (S), para que siga pareciendo un renglón.
        const qreal cap = curve.shape == CurveShape::C ? curve.length / 4 : curve.length / 8;
        curve.height = side * bend * std::min(WritingCurve::maxHeight(curve.shape, curve.length, minRadius), cap);
        base = curve.sample(0);
        xLine = curve.sample(xHeight);
        top = curve.sample(spacing);
        // Centro y alcance del bloque, para que entre en la zona.
        qreal minY = 1e9, maxY = -1e9;
        for (const QList<QPointF>* line : {&base, &top})
            for (const QPointF& p : *line) {
                minY = std::min(minY, p.y());
                maxY = std::max(maxY, p.y());
            }
        blockCenter = QPointF(0, (minY + maxY) / 2);
        reach = 0;
        for (const QList<QPointF>* line : {&base, &top})
            for (const QPointF& p : *line)
                reach = std::max(reach, QLineF(p, blockCenter).length());
        if (reach <= zone.radius - margin || curve.length < 10)
            break;
        curve.length *= 0.95;
    }
    const qreal room = std::max(0.0, zone.radius - margin - reach);
    const qreal along = rng.generateDouble() * 2 * std::numbers::pi;
    const QPointF where = zone.center + QPointF(std::cos(along), std::sin(along)) * room * std::sqrt(rng.generateDouble());
    const auto toSheet = [&](const QList<QPointF>& local) {
        QList<QPointF> out;
        out.reserve(local.size());
        for (const QPointF& p : local) {
            const QPointF q = p - blockCenter;
            out.append(where + u * q.x() + n * q.y());
        }
        return out;
    };
    const QPainterPath basePath = polylinePath(toSheet(base));
    const QPainterPath xPath = polylinePath(toSheet(xLine));
    const QPainterPath topPath = polylinePath(toSheet(top));

    Generated out;
    out.ideal = {basePath, xPath, topPath};
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionPath(painter, topPath);
    appkit::guides::constructionPath(painter, xPath);
    appkit::guides::guidePath(painter, basePath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(appkit::theme::kGuia);
    painter.drawEllipse(QPointF(basePath.elementAt(0)), 4.5, 4.5); // dónde empezar
    return out;
}

} // namespace ejercicios
