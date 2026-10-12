#include "exercises/CilindroQueGira.h"

#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace ejercicios {

const QString CilindroQueGira::kSeam = QStringLiteral("seam");

namespace cilindro {

using giro::Vec3;

View project(const Shape& shape, Axis axis, double degrees, double eye)
{
    const double r = shape.radius, h = shape.length / 2;
    const auto place = [&](Vec3 p) { return giro::place(p, axis, degrees); };
    // El costado en el ángulo t alrededor del eje: normal (cos t, 0, sin t).
    const auto radial = [](double t) { return Vec3{std::cos(t), 0, std::sin(t)}; };
    const auto at = [&](double t, double y) { return Vec3{r * std::cos(t), y, r * std::sin(t)}; };
    const auto seen = [&](double t, double y) { return giro::facesCamera(place(radial(t)), place(at(t, y)), eye); };
    const auto show = [&](Vec3 p) { return giro::project(place(p), eye); };

    View view;
    for (int cap = 0; cap < 2; ++cap) {
        const double y = cap ? h : -h;
        const Vec3 up = place({0, cap ? 1.0 : -1.0, 0});
        const bool capFront = giro::facesCamera(up, {up.x * h, up.y * h, up.z * h}, eye);
        for (int i = 0; i <= kRimSteps; ++i) {
            const double t = 2 * std::numbers::pi * i / kRimSteps;
            view.rims[cap].push_back(show(at(t, y)));
            if (i < kRimSteps) {
                const double mid = 2 * std::numbers::pi * (i + 0.5) / kRimSteps;
                view.rimVisible[cap].push_back(capFront || seen(mid, y));
            }
        }
        if (cap == 1) {
            const double t = std::numbers::pi / 2;
            view.mark = {show(at(t, h)), show({kMarkRatio * r * std::cos(t), h, kMarkRatio * r * std::sin(t)}), capFront};
        }
    }
    // La silueta: el costado de canto, n(t)·(ojo − p) = 0. Como n(t)·p = r en todo el largo, queda
    // A cos t + B sin t = r, con A y B las z de las normales en t = 0 y t = 90° por el ojo.
    const double a = place({1, 0, 0}).z * eye, b = place({0, 0, 1}).z * eye;
    const double reach = std::hypot(a, b);
    if (reach > r) {
        const double base = std::atan2(b, a), spread = std::acos(r / reach);
        for (const double t : {base + spread, base - spread})
            view.silhouette.push_back({show(at(t, -h)), show(at(t, h)), true});
    }
    const double t = std::numbers::pi / 2;
    view.seam = {show(at(t, -h)), show(at(t, h)), seen(t, 0)};
    return view;
}

Draw draw(quint32 seed, int axisParam)
{
    QRandomGenerator rng(seed);
    Draw out;
    // Largo sobre diámetro de 1:3 a 3:1, parejo en escala logarítmica.
    const double ratio = std::pow(3.0, 2 * rng.generateDouble() - 1);
    const double radius = 1, length = 2 * radius * ratio;
    const double sphere = std::hypot(radius, length / 2);
    out.shape = {radius / sphere, length / sphere};
    out.axis = giro::axisFor(axisParam, rng);
    out.startDeg = 5 * int(rng.bounded(72)); // no siempre en el mismo lugar
    return out;
}

} // namespace cilindro

QString CilindroQueGira::id() const
{
    return QStringLiteral("cilindroQueGira");
}

QString CilindroQueGira::title() const
{
    return QStringLiteral("Cilindro que gira");
}

QList<appkit::Param> CilindroQueGira::params() const
{
    // Por defecto sobre el eje horizontal: da vueltas de punta y las tapas cambian de elipse.
    QList<appkit::Param> all = turnParams(1, QStringLiteral("Contorno oculto (punteado)"));
    all.append({.key = kSeam, .label = QStringLiteral("Costura (deja ver el giro)"), .type = appkit::Param::Type::Toggle,
                .defaultValue = true});
    return all;
}

StackGenerated CilindroQueGira::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const cilindro::Draw d = cilindro::draw(seed, v.value(kAxis).toInt());
    const int step = v.value(kStep).toInt();
    const bool seam = v.value(kSeam).toBool();
    const giro::Frame frame = giro::frame(v, zone);
    const double size = 2 * frame.radius; // el diámetro de la esfera que lo envuelve, en mm
    const cilindro::Shape shape{d.shape.radius * size / 2, d.shape.length * size / 2};

    StackGenerated out;
    for (int i = 0; i < sheetCount(v); ++i) {
        const int degrees = (d.startDeg + i * step) % 360;
        const cilindro::View view = cilindro::project(shape, d.axis, degrees, frame.eye);
        Generated g;
        {
            giro::Ghost ghost(g, frame, zone, v.value(kHidden).toBool());
            for (int cap = 0; cap < 2; ++cap)
                ghost.curve(view.rims[cap], view.rimVisible[cap]);
            for (const cilindro::Segment& s : view.silhouette)
                ghost.line(s.a, s.b, s.visible);
            if (seam) {
                ghost.line(view.seam.a, view.seam.b, view.seam.visible);
                ghost.line(view.mark.a, view.mark.b, view.mark.visible);
            }
            ghost.angle(degrees);
        }
        out.sheets.append(g);
    }
    return out;
}

} // namespace ejercicios
