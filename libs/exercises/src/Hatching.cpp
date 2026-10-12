#include "exercises/Hatching.h"

#include <appkit/Guides.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>
#include <optional>

namespace ejercicios {

namespace {

const QString kSize = QStringLiteral("size");
const QString kSpacing = QStringLiteral("spacing");
const QString kRandomAngle = QStringLiteral("randomAngle");
const QString kAngle = QStringLiteral("angle");

// La recta p + t·d recortada al rectángulo |x| ≤ hw, |y| ≤ hh (Liang–Barsky).
std::optional<QLineF> clipToRect(QPointF p, QPointF d, qreal hw, qreal hh)
{
    qreal t0 = -1e9, t1 = 1e9;
    const auto clip = [&](qreal pos, qreal dir, qreal half) {
        if (std::abs(dir) < 1e-12)
            return std::abs(pos) <= half;
        qreal a = (-half - pos) / dir, b = (half - pos) / dir;
        if (a > b)
            std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        return t0 < t1;
    };
    if (!clip(p.x(), d.x(), hw) || !clip(p.y(), d.y(), hh))
        return std::nullopt;
    return QLineF(p + t0 * d, p + t1 * d);
}

} // namespace

QString Hatching::id() const
{
    return QStringLiteral("hatching");
}

QString Hatching::title() const
{
    return QStringLiteral("Hatching");
}

QString Hatching::group() const
{
    return QStringLiteral("Líneas");
}

QList<appkit::Param> Hatching::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kSize, .label = QStringLiteral("Tamaño del área (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.2, .maximum = 0.8, .step = 0.05, .defaultValue = 0.45},
        {.key = kSpacing, .label = QStringLiteral("Espaciado sugerido"), .type = Type::Real, .minimum = 2, .maximum = 10,
         .step = 0.5, .defaultValue = 4.0, .decimals = 1, .suffix = QStringLiteral(" mm")},
        {.key = kRandomAngle, .label = QStringLiteral("Ángulo al azar"), .type = Type::Toggle, .defaultValue = true},
        {.key = kAngle, .label = QStringLiteral("Ángulo fijo (respecto del área)"), .type = Type::Integer, .minimum = 0,
         .maximum = 165, .step = 15, .defaultValue = 45, .suffix = QStringLiteral("°")},
    };
}

Generated Hatching::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);

    // Rectángulo en el marco del ejercicio (la orientación de la zona lo rota): lado mayor
    // `size` del diámetro, el otro entre 55 % y 100 % de ese; achicado si no entra.
    qreal w = v.value(kSize).toDouble() * 2 * zone.radius;
    qreal h = w * (0.55 + 0.45 * rng.generateDouble());
    const qreal margin = 4;
    const qreal halfDiagonal = std::hypot(w, h) / 2;
    if (halfDiagonal > zone.radius - margin) {
        const qreal k = (zone.radius - margin) / halfDiagonal;
        w *= k;
        h *= k;
    }
    // Corrimiento al azar mientras las cuatro esquinas sigan en la zona.
    const qreal room = zone.radius - margin - std::hypot(w, h) / 2;
    const qreal along = rng.generateDouble() * 2 * std::numbers::pi;
    const QPointF center = room > 0 ? QPointF(std::cos(along), std::sin(along)) * room * std::sqrt(rng.generateDouble())
                                    : QPointF();

    const int steps = int(rng.bounded(12)); // 0°..165° (se sortea siempre: misma semilla, mismo resto)
    const int degrees = v.value(kRandomAngle).toBool() ? 15 * steps : v.value(kAngle).toInt();
    const qreal theta = degrees * std::numbers::pi / 180;
    const QPointF dir(std::cos(theta), std::sin(theta));
    const QPointF normal(-dir.y(), dir.x());

    // Espaciado en píxeles de la hoja; no más de un tercio del rectángulo para que la
    // segunda muestra entre.
    const qreal spacing = std::min(zone.mm(v.value(kSpacing).toDouble()), std::min(w, h) / 3);
    const QLineF first = *clipToRect(QPointF(), dir, w / 2, h / 2);
    QLineF second = clipToRect(normal * spacing, dir, w / 2, h / 2).value_or(first);
    // La segunda, corta: el 40 % del medio.
    second = QLineF(second.pointAt(0.3), second.pointAt(0.7));

    const auto toCanvas = [&](QPointF local) { return zone.toCanvas(center + local); };
    const QPointF corners[4] = {toCanvas({-w / 2, -h / 2}), toCanvas({w / 2, -h / 2}), toCanvas({w / 2, h / 2}),
                                toCanvas({-w / 2, h / 2})};
    const QLineF sample1(toCanvas(first.p1()), toCanvas(first.p2()));
    const QLineF sample2(toCanvas(second.p1()), toCanvas(second.p2()));

    Generated out;
    QPainterPath contour(corners[0]);
    for (int i = 1; i < 4; ++i)
        contour.lineTo(corners[i]);
    contour.closeSubpath();
    out.ideal.append(contour);
    for (const QLineF& line : {sample1, sample2}) {
        QPainterPath path(line.p1());
        path.lineTo(line.p2());
        out.ideal.append(path);
    }
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < 4; ++i)
        appkit::guides::constructionLine(painter, corners[i], corners[(i + 1) % 4]);
    appkit::guides::guideLine(painter, sample1.p1(), sample1.p2());
    appkit::guides::guideLine(painter, sample2.p1(), sample2.p2());
    return out;
}

} // namespace ejercicios
