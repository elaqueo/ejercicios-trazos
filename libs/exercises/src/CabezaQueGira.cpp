#include "exercises/CabezaQueGira.h"

#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>

namespace ejercicios {

namespace cabeza {

using giro::Vec3;

namespace {

constexpr double kTwoPi = 2 * std::numbers::pi;

double length(Vec3 v)
{
    return std::sqrt(giro::dot(v, v));
}

} // namespace

View project(const Shape& shape, Axis axis, double degrees, double eye)
{
    const double R = shape.skull, rc = shape.cut, dc = std::sqrt(R * R - rc * rc);
    const auto place = [&](Vec3 p) { return giro::place(p, axis, degrees); };
    const auto show = [&](Vec3 p) { return giro::project(place(p), eye); };
    // Un punto de la esfera se ve si su normal (él mismo) mira a la cámara.
    const auto onSphere = [&](Vec3 p) {
        const Vec3 q = place(p);
        return giro::facesCamera(q, q, eye);
    };
    const auto circle = [&](const std::function<Vec3(double)>& at, const std::function<bool(double)>& seen) {
        Curve c;
        for (int i = 0; i <= kSteps; ++i) {
            c.points.push_back(show(at(kTwoPi * i / kSteps)));
            if (i < kSteps)
                c.visible.push_back(seen(kTwoPi * (i + 0.5) / kSteps));
        }
        return c;
    };

    View view;
    // El contorno: con el centro en el eje, los rayos tangentes tocan la esfera en el círculo
    // z = R²/eye, de radio R·√(1 − R²/eye²); proyectado, un círculo de radio R·eye/√(eye² − R²).
    const double contour = R * eye / std::sqrt(eye * eye - R * R);
    for (int i = 0; i <= kSteps; ++i) {
        const double a = kTwoPi * i / kSteps;
        view.outline.points.push_back({contour * std::cos(a), contour * std::sin(a)});
        if (i < kSteps)
            view.outline.visible.push_back(true);
    }
    const auto middle = [&](double a) { return Vec3{0, R * std::sin(a), R * std::cos(a)}; };
    view.middle = circle(middle, [&](double a) { return onSphere(middle(a)); });
    const auto brow = [&](double a) { return Vec3{R * std::sin(a), 0, R * std::cos(a)}; };
    view.brow = circle(brow, [&](double a) { return onSphere(brow(a)); });
    for (int side = 0; side < 2; ++side) {
        const double sx = side ? 1.0 : -1.0;
        const Vec3 n = place({sx, 0, 0});
        const bool front = giro::facesCamera(n, {n.x * dc, n.y * dc, n.z * dc}, eye);
        view.cuts[size_t(side)] = circle([&](double a) { return Vec3{sx * dc, rc * std::sin(a), rc * std::cos(a)}; },
                                         [front](double) { return front; });
    }
    // La cara: planos aproximados, el de adelante (un poco hacia abajo) y los de la mandíbula.
    const Vec3 nose{0, -rc, dc}, chin{0, -shape.chinDrop, shape.chinForward};
    view.face = {show(nose), show(chin), giro::facesCamera(place({0, -0.15, 1}), place(nose), eye)};
    for (int side = 0; side < 2; ++side) {
        const double sx = side ? 1.0 : -1.0;
        const Vec3 start{sx * dc, -0.5 * R, -0.2 * R};
        view.jaw[size_t(side)] = {show(start), show(chin), giro::facesCamera(place({sx, -0.4, 0.5}), place(start), eye)};
    }
    return view;
}

Draw draw(quint32 seed, int axisParam)
{
    QRandomGenerator rng(seed);
    Draw out;
    // Poca variación: el corte (la oreja) y el largo de la cara.
    Shape s;
    s.cut = 0.58 + 0.08 * rng.generateDouble();
    s.chinDrop = 1.3 * (0.9 + 0.2 * rng.generateDouble());
    // Todo dentro de la esfera de radio 1 alrededor del pivote: lo más lejos es el mentón.
    const double reach = std::max(1.0, length({0, s.chinDrop, s.chinForward}));
    out.shape = {s.skull / reach, s.cut / reach, s.chinDrop / reach, s.chinForward / reach};
    out.axis = giro::axisFor(axisParam, rng);
    out.startDeg = 5 * int(rng.bounded(72));
    return out;
}

} // namespace cabeza

QString CabezaQueGira::id() const
{
    return QStringLiteral("cabezaQueGira");
}

QString CabezaQueGira::title() const
{
    return QStringLiteral("Cabeza que gira");
}

QList<appkit::Param> CabezaQueGira::params() const
{
    // Por defecto sobre el eje vertical: gira de un lado al otro.
    return turnParams(0, QStringLiteral("Lo de atrás (punteado)"));
}

StackGenerated CabezaQueGira::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const cabeza::Draw d = cabeza::draw(seed, v.value(kAxis).toInt());
    const int step = v.value(kStep).toInt();
    const giro::Frame frame = giro::frame(v, zone);
    const double k = frame.radius;
    const cabeza::Shape shape{d.shape.skull * k, d.shape.cut * k, d.shape.chinDrop * k, d.shape.chinForward * k};

    StackGenerated out;
    for (int i = 0; i < sheetCount(v); ++i) {
        const int degrees = (d.startDeg + i * step) % 360;
        const cabeza::View view = cabeza::project(shape, d.axis, degrees, frame.eye);
        Generated g;
        {
            giro::Ghost ghost(g, frame, zone, v.value(kHidden).toBool());
            for (const cabeza::Curve* c : {&view.outline, &view.middle, &view.brow, &view.cuts[0], &view.cuts[1]})
                ghost.curve(c->points, c->visible);
            ghost.line(view.face.a, view.face.b, view.face.visible);
            for (const cabeza::Segment& s : view.jaw)
                ghost.line(s.a, s.b, s.visible);
            ghost.angle(degrees);
        }
        out.sheets.append(g);
    }
    return out;
}

} // namespace ejercicios
