#include "exercises/Presion.h"

#include "exercises/WritingCurve.h"

#include <appkit/Theme.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kRamp = QStringLiteral("profileRamp");
const QString kConstant = QStringLiteral("profileConstant");
const QString kFree = QStringLiteral("profileFree");
const QString kLength = QStringLiteral("length");
const QString kWidth = QStringLiteral("width");
const QString kCurved = QStringLiteral("curved");

constexpr double kMinFraction = 0.2; // el ancho mínimo, como fracción del máximo

enum class Profile { Ramp, Constant, Free };

} // namespace

QString Presion::id() const
{
    return QStringLiteral("presion");
}

QString Presion::title() const
{
    return QStringLiteral("Control de presión");
}

QString Presion::group() const
{
    return QStringLiteral("Memoria y presión"); // un grupo en el menú (mesa del diseño, HU-77)
}

QList<appkit::Param> Presion::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kRamp, .label = QStringLiteral("Fino → grueso"), .type = Type::Toggle, .defaultValue = true},
        {.key = kConstant, .label = QStringLiteral("Grosor constante"), .type = Type::Toggle, .defaultValue = true},
        {.key = kFree, .label = QStringLiteral("Perfil libre"), .type = Type::Toggle, .defaultValue = true},
        {.key = kLength, .label = QStringLiteral("Largo (× diámetro de la zona)"), .type = Type::Real, .minimum = 0.4,
         .maximum = 0.9, .step = 0.05, .defaultValue = 0.7},
        {.key = kWidth, .label = QStringLiteral("Ancho máximo de la banda"), .type = Type::Real, .minimum = 4,
         .maximum = 15, .step = 0.5, .defaultValue = 10.0, .decimals = 1, .suffix = QStringLiteral(" mm")},
        {.key = kCurved, .label = QStringLiteral("Trazo curvo"), .type = Type::Toggle, .defaultValue = false},
    };
}

Generated Presion::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    QList<Profile> profiles;
    if (v.value(kRamp).toBool())
        profiles.append(Profile::Ramp);
    if (v.value(kConstant).toBool())
        profiles.append(Profile::Constant);
    if (v.value(kFree).toBool())
        profiles.append(Profile::Free);
    if (profiles.isEmpty())
        profiles = {Profile::Ramp, Profile::Constant, Profile::Free};
    const Profile profile = profiles[int(rng.bounded(quint32(profiles.size())))];

    const double wMax = zone.mm(v.value(kWidth).toDouble());
    const double wMin = kMinFraction * wMax;
    // Perfil libre: nudos que alternan fino y grueso, unidos con coseno (suave, sin saltos y
    // siempre entre el mínimo y el máximo).
    const int knots = 4 + int(rng.bounded(2)); // 2 o 3 subidas y bajadas
    const bool startHigh = rng.bounded(2) == 1;
    QList<double> knotWidth;
    for (int i = 0; i < knots; ++i) {
        const bool high = (i % 2 == 0) == startHigh;
        const double range = wMax - wMin;
        knotWidth.append(high ? wMax - 0.35 * range * rng.generateDouble() : wMin + 0.35 * range * rng.generateDouble());
    }
    const auto widthAt = [&](double t) {
        switch (profile) {
        case Profile::Ramp: return wMin + (wMax - wMin) * t;
        case Profile::Constant: return (wMin + wMax) / 2;
        case Profile::Free:
        default: {
            const double x = t * (knots - 1);
            const int i = std::min(int(x), knots - 2);
            const double s = (1 - std::cos((x - i) * std::numbers::pi)) / 2;
            return knotWidth[i] + (knotWidth[i + 1] - knotWidth[i]) * s;
        }
        }
    };

    // El recorrido en el marco del ejercicio (la orientación de la zona lo rota): recto o una
    // C suave; se acorta hasta que la banda entra en la zona y se corre al azar lo que sobra.
    WritingCurve curve;
    curve.shape = CurveShape::C;
    const double bendSide = rng.bounded(2) ? 1.0 : -1.0;
    double length = v.value(kLength).toDouble() * 2 * zone.radius;
    QList<QPointF> center, left, right;
    double reach = 0;
    const double margin = 6;
    for (;;) {
        curve.length = length;
        curve.height = v.value(kCurved).toBool() ? bendSide * 0.12 * length : 0;
        center = curve.sample(0, kSamples);
        left.clear();
        right.clear();
        const double shift = curve.height / 2; // centra la panza en el origen
        for (int i = 0; i <= kSamples; ++i) {
            const double t = double(i) / kSamples;
            const double x = -length / 2 + length * t;
            const double m = curve.slope(x);
            const QPointF n = QPointF(-m, 1) / std::sqrt(1 + m * m);
            center[i] -= QPointF(0, shift);
            left.append(center[i] + n * widthAt(t) / 2);
            right.append(center[i] - n * widthAt(t) / 2);
        }
        reach = 0;
        for (const QList<QPointF>* side : {&left, &right})
            for (const QPointF& p : *side)
                reach = std::max(reach, std::hypot(p.x(), p.y()));
        if (reach <= zone.radius - margin || length < 20)
            break;
        length *= 0.95;
    }
    const double room = std::max(0.0, zone.radius - margin - reach);
    const double along = rng.generateDouble() * 2 * std::numbers::pi;
    const QPointF offset = QPointF(std::cos(along), std::sin(along)) * room * std::sqrt(rng.generateDouble());
    const auto toCanvas = [&](const QList<QPointF>& local) {
        QList<QPointF> out;
        for (const QPointF& p : local)
            out.append(zone.toCanvas(offset + p));
        return out;
    };
    const QList<QPointF> c = toCanvas(center), l = toCanvas(left), r = toCanvas(right);

    Generated out;
    out.ideal.append(polylinePath(c));
    QPainterPath band(l.first());
    for (int i = 1; i < l.size(); ++i)
        band.lineTo(l[i]);
    for (int i = int(r.size()) - 1; i >= 0; --i)
        band.lineTo(r[i]);
    band.closeSubpath();
    out.ideal.append(band);

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor fill = appkit::theme::kGuiaSuave;
    fill.setAlpha(110);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawPath(band);
    painter.setPen(QPen(appkit::theme::kGuia, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(out.ideal.first());
    painter.setPen(Qt::NoPen);
    painter.setBrush(appkit::theme::kGuia);
    painter.drawEllipse(c.first(), 5.0, 5.0); // el arranque
    return out;
}

} // namespace ejercicios
