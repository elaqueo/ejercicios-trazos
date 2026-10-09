#include "exercises/Parrafo.h"

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

const QString kLines = QStringLiteral("lines");
const QString kLength = QStringLiteral("length");
const QString kSpacing = QStringLiteral("spacing");
const QString kXHeight = QStringLiteral("xHeight");
const QString kMinRadius = QStringLiteral("minRadius");
const QString kAllowS = QStringLiteral("allowS");
const QString kRandomAngle = QStringLiteral("randomAngle");
const QString kAngle = QStringLiteral("angle");

} // namespace

QString Parrafo::id() const
{
    return QStringLiteral("parrafo");
}

QString Parrafo::title() const
{
    return QStringLiteral("Escribir sobre varias curvas");
}

QString Parrafo::group() const
{
    return QStringLiteral("Escritura");
}

QList<appkit::Param> Parrafo::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kLines, .label = QStringLiteral("Renglones"), .type = Type::Integer, .minimum = 2, .maximum = 6,
         .step = 1, .defaultValue = 4},
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

Generated Parrafo::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const qreal randomDegrees = rng.generateDouble() * 40 - 20;
    const bool sShape = rng.bounded(2) == 1;
    const qreal bend = 0.4 + 0.6 * rng.generateDouble();
    const qreal side = rng.bounded(2) ? 1.0 : -1.0;

    const int lines = v.value(kLines).toInt();
    const qreal degrees = v.value(kRandomAngle).toBool() ? randomDegrees : v.value(kAngle).toDouble();
    const qreal theta = degrees * std::numbers::pi / 180;
    const QPointF u(std::cos(theta), -std::sin(theta));
    const QPointF n(-std::sin(theta), -std::cos(theta));

    const qreal spacing = zone.mm(v.value(kSpacing).toDouble());
    const qreal xHeight = v.value(kXHeight).toDouble() * spacing;
    const qreal minRadius = std::max(zone.mm(v.value(kMinRadius).toDouble()), 1.5 * spacing);
    // Los renglones se alejan de la base hasta (lines − 1) interlineados hacia abajo, y el
    // techo del primero uno hacia arriba: la base deja ese margen de radio.
    const qreal baseRadius = minRadius + std::max(lines - 1, 1) * spacing;

    WritingCurve curve;
    curve.shape = sShape && v.value(kAllowS).toBool() ? CurveShape::S : CurveShape::C;
    curve.length = v.value(kLength).toDouble() * 2 * zone.radius;
    QList<QList<QPointF>> bases, xLines;
    QList<QPointF> top;
    QPointF blockCenter;
    qreal reach = 0;
    const qreal margin = 8;
    for (;;) {
        const qreal cap = curve.shape == CurveShape::C ? curve.length / 4 : curve.length / 8;
        curve.height = side * bend * std::min(WritingCurve::maxHeight(curve.shape, curve.length, baseRadius), cap);
        bases.clear();
        xLines.clear();
        for (int k = 0; k < lines; ++k) {
            bases.append(curve.sample(-k * spacing));
            xLines.append(curve.sample(-k * spacing + xHeight));
        }
        top = curve.sample(spacing);
        qreal minY = 1e9, maxY = -1e9;
        for (const QList<QPointF>* line : {&top, &bases.last()})
            for (const QPointF& p : *line) {
                minY = std::min(minY, p.y());
                maxY = std::max(maxY, p.y());
            }
        blockCenter = QPointF(0, (minY + maxY) / 2);
        reach = 0;
        for (const QList<QPointF>* line : {&top, &bases.last()})
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
        return polylinePath(out);
    };

    Generated out;
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionPath(painter, toSheet(top));
    for (int k = 0; k < lines; ++k) {
        const QPainterPath base = toSheet(bases[k]);
        const QPainterPath x = toSheet(xLines[k]);
        out.ideal.append(base);
        out.ideal.append(x);
        appkit::guides::constructionPath(painter, x);
        appkit::guides::guidePath(painter, base);
        painter.setPen(Qt::NoPen);
        painter.setBrush(appkit::theme::kGuia);
        painter.drawEllipse(QPointF(base.elementAt(0)), 4.5, 4.5); // dónde empezar cada renglón
    }
    return out;
}

} // namespace ejercicios
