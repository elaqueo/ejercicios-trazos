#include "exercises/ConoQueGira.h"

#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace ejercicios {

const QString ConoQueGira::kSeam = QStringLiteral("seam");

namespace cono {

using giro::Vec3;

View project(const Shape& shape, Axis axis, double degrees, double eye)
{
    const double r = shape.radius, h = shape.height;
    const auto place = [&](Vec3 p) { return giro::place(p, axis, degrees); };
    const auto show = [&](Vec3 p) { return giro::project(place(p), eye); };
    const auto at = [&](double t) { return Vec3{r * std::cos(t), -h / 2, r * std::sin(t)}; };
    // El costado en t tiene normal h·(cos t, 0, sin t) + r·(0, 1, 0), y toda la generatriz pasa por
    // el vértice: mira a la cámara si n(t)·(ojo − vértice) > 0. Con las direcciones ya ubicadas
    // queda A cos t + B sin t + r·(ey·w) > 0.
    const Vec3 ex = place({1, 0, 0}), ey = place({0, 1, 0}), ez = place({0, 0, 1});
    const Vec3 apex = place({0, h / 2, 0});
    const Vec3 w{-apex.x, -apex.y, eye - apex.z};
    const double a = h * giro::dot(ex, w), b = h * giro::dot(ez, w), c = r * giro::dot(ey, w);
    const auto seen = [&](double t) { return a * std::cos(t) + b * std::sin(t) + c > 0; };
    const Vec3 down = place({0, -1, 0});
    const bool baseFront = giro::facesCamera(down, {down.x * h / 2, down.y * h / 2, down.z * h / 2}, eye);

    View view;
    view.apex = show({0, h / 2, 0});
    for (int i = 0; i <= kRimSteps; ++i) {
        view.rim.push_back(show(at(2 * std::numbers::pi * i / kRimSteps)));
        if (i < kRimSteps)
            view.rimVisible.push_back(baseFront || seen(2 * std::numbers::pi * (i + 0.5) / kRimSteps));
    }
    // La silueta: A cos t + B sin t = −C (de canto), si tiene solución.
    const double reach = std::hypot(a, b);
    if (reach > std::abs(c)) {
        const double base = std::atan2(b, a), spread = std::acos(-c / reach);
        for (const double t : {base + spread, base - spread})
            view.silhouette.push_back({show(at(t)), view.apex, true});
    }
    const double t = std::numbers::pi / 2;
    view.seam = {show(at(t)), view.apex, seen(t)};
    view.mark = {show(at(t)), show({kMarkRatio * r * std::cos(t), -h / 2, kMarkRatio * r * std::sin(t)}), baseFront};
    return view;
}

Draw draw(quint32 seed, int axisParam)
{
    QRandomGenerator rng(seed);
    Draw out;
    // Altura sobre diámetro de 1:3 a 3:1, parejo en escala logarítmica.
    const double ratio = std::pow(3.0, 2 * rng.generateDouble() - 1);
    const double radius = 1, height = 2 * radius * ratio;
    const double sphere = std::hypot(radius, height / 2); // el borde de la base, lo más lejos del centro
    out.shape = {radius / sphere, height / sphere};
    out.axis = giro::axisFor(axisParam, rng);
    out.startDeg = 5 * int(rng.bounded(72));
    return out;
}

} // namespace cono

QString ConoQueGira::id() const
{
    return QStringLiteral("conoQueGira");
}

QString ConoQueGira::title() const
{
    return QStringLiteral("Cono que gira");
}

QList<appkit::Param> ConoQueGira::params() const
{
    // Por defecto sobre el eje horizontal: da vueltas de punta.
    QList<appkit::Param> all = turnParams(1, QStringLiteral("Contorno oculto (punteado)"));
    all.append({.key = kSeam, .label = QStringLiteral("Costura (deja ver el giro)"), .type = appkit::Param::Type::Toggle,
                .defaultValue = true});
    return all;
}

StackGenerated ConoQueGira::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const cono::Draw d = cono::draw(seed, v.value(kAxis).toInt());
    const int step = v.value(kStep).toInt();
    const bool seam = v.value(kSeam).toBool();
    const giro::Frame frame = giro::frame(v, zone);
    const cono::Shape shape{d.shape.radius * frame.radius, d.shape.height * frame.radius};

    StackGenerated out;
    for (int i = 0; i < sheetCount(v); ++i) {
        const int degrees = (d.startDeg + i * step) % 360;
        const cono::View view = cono::project(shape, d.axis, degrees, frame.eye);
        Generated g;
        {
            giro::Ghost ghost(g, frame, zone, v.value(kHidden).toBool());
            ghost.curve(view.rim, view.rimVisible);
            for (const cono::Segment& s : view.silhouette)
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
