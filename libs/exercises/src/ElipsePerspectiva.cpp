#include "exercises/ElipsePerspectiva.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace ejercicios {

namespace {

const QString kFloor = QStringLiteral("floor");
const QString kWall = QStringLiteral("wall");
const QString kPf1 = QStringLiteral("pf1");
const QString kPf2 = QStringLiteral("pf2");
const QString kSize = QStringLiteral("size");
const QString kEye = QStringLiteral("eye");

constexpr int kEllipseSamples = 96; // múltiplo de 4: incluye los cuatro puntos de tangencia
constexpr qreal kMargin = 40;

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(double k) const { return {x * k, y * k, z * k}; }
};

// La cámara: ojo en el origen mirando hacia +z, y hacia arriba, horizonte a la altura del ojo.
struct Camera {
    double cx = 0, horizon = 0, f = 1;
    QPointF project(const Vec3& p) const { return {cx + f * p.x / p.z, horizon - f * p.y / p.z}; }
    // El PF de una dirección (si no es paralela a la hoja).
    std::optional<QPointF> vanishing(const Vec3& d) const
    {
        if (std::abs(d.z) < 1e-9)
            return std::nullopt;
        return QPointF(cx + f * d.x / d.z, horizon - f * d.y / d.z);
    }
    // El punto 3D a profundidad z que se ve en `screen`.
    Vec3 unproject(QPointF screen, double z) const
    {
        return {(screen.x() - cx) * z / f, (horizon - screen.y()) * z / f, z};
    }
};

QPainterPath point(QPointF p)
{
    QPainterPath path;
    path.moveTo(p);
    return path;
}

} // namespace

QString ElipsePerspectiva::id() const
{
    return QStringLiteral("elipsePerspectiva");
}

QString ElipsePerspectiva::title() const
{
    return QStringLiteral("Elipses en perspectiva");
}

QString ElipsePerspectiva::group() const
{
    return QStringLiteral("Elipses");
}

QList<appkit::Param> ElipsePerspectiva::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kFloor, .label = QStringLiteral("Piso / techo"), .type = Type::Toggle, .defaultValue = true},
        {.key = kWall, .label = QStringLiteral("Pared"), .type = Type::Toggle, .defaultValue = true},
        {.key = kPf1, .label = QStringLiteral("1 punto de fuga"), .type = Type::Toggle, .defaultValue = true},
        {.key = kPf2, .label = QStringLiteral("2 puntos de fuga"), .type = Type::Toggle, .defaultValue = true},
        {.key = kSize, .label = QStringLiteral("Tamaño (× alto de la hoja)"), .type = Type::Real, .minimum = 0.2,
         .maximum = 0.6, .step = 0.05, .defaultValue = 0.35},
        {.key = kEye, .label = QStringLiteral("Distancia del ojo (× ancho de la hoja)"), .type = Type::Real,
         .minimum = 0.3, .maximum = 1.0, .step = 0.05, .defaultValue = 0.5},
    };
}

Generated ElipsePerspectiva::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const bool floorOk = v.value(kFloor).toBool(), wallOk = v.value(kWall).toBool();
    const bool horizontal = floorOk == wallOk ? rng.bounded(2) == 0 : floorOk;
    const bool pf1Ok = v.value(kPf1).toBool(), pf2Ok = v.value(kPf2).toBool();
    const bool onePoint = pf1Ok == pf2Ok ? rng.bounded(2) == 0 : pf1Ok;
    const bool below = rng.bounded(2) == 0; // piso (abajo del horizonte) o techo
    const double alpha = (20 + 50 * rng.generateDouble()) * std::numbers::pi / 180 * (rng.bounded(2) ? 1 : -1);

    const QRectF sheet = zone.sheet.isValid() ? zone.sheet
                                              : QRectF(zone.center - QPointF(zone.radius, zone.radius),
                                                       QSizeF(2 * zone.radius, 2 * zone.radius));
    const QRectF inner = sheet.adjusted(kMargin, kMargin, -kMargin, -kMargin);
    Camera camera;
    camera.cx = sheet.center().x();
    camera.f = v.value(kEye).toDouble() * sheet.width();
    camera.horizon = sheet.top() + sheet.height() * (horizontal ? (below ? 0.25 + 0.1 * rng.generateDouble()
                                                                         : 0.65 + 0.1 * rng.generateDouble())
                                                                : 0.4 + 0.2 * rng.generateDouble());

    // Los ejes del cuadrado en 3D.
    Vec3 u, w;
    if (horizontal) {
        const double a = onePoint ? 0 : alpha;
        u = {std::cos(a), 0, std::sin(a)};
        w = {-std::sin(a), 0, std::cos(a)};
    } else {
        // La pared: con 1 PF, de costado (fuga al centro); con 2 PF, girada.
        const double a = onePoint ? std::numbers::pi / 2 : alpha;
        u = {std::cos(a), 0, std::sin(a)};
        w = {0, 1, 0};
    }

    // Centro aparente al azar; de ahí el centro 3D y el lado que da el tamaño pedido. Si no
    // entra en la hoja (o cruza el horizonte), se achica, y si no alcanza, otro centro.
    const qreal target = v.value(kSize).toDouble() * sheet.height();
    std::array<Vec3, 4> corners3;
    std::array<QPointF, 4> corners;
    Vec3 center3;
    double half = 0;
    bool placed = false;
    for (int attempt = 0; attempt < 100 && !placed; ++attempt) {
        QPointF screen(inner.left() + inner.width() * (0.25 + 0.5 * rng.generateDouble()), 0);
        if (horizontal) {
            const qreal gap = 0.1 * sheet.height() + target / 2; // lejos del horizonte
            const qreal lo = below ? camera.horizon + gap : inner.top() + target / 2;
            const qreal hi = below ? inner.bottom() - target / 2 : camera.horizon - gap;
            if (hi <= lo)
                continue;
            screen.setY(lo + (hi - lo) * rng.generateDouble());
            // El plano: el ojo a altura 1 sobre el piso (o bajo el techo).
            const double planeY = below ? -1.0 : 1.0;
            const double z = camera.f * std::abs(planeY) / std::abs(screen.y() - camera.horizon);
            center3 = camera.unproject(screen, z);
            center3.y = planeY;
        } else {
            screen.setY(inner.top() + inner.height() * (0.25 + 0.5 * rng.generateDouble()));
            center3 = camera.unproject(screen, 3.0);
        }
        half = target / 2 * center3.z / camera.f;
        for (int shrink = 0; shrink < 30 && !placed; ++shrink) {
            if (shrink > 0)
                half *= 0.9;
            const std::array<double, 4> su{-1, 1, 1, -1}, sw{-1, -1, 1, 1};
            bool ok = true;
            for (int i = 0; i < 4; ++i) {
                corners3[i] = center3 + u * (su[i] * half) + w * (sw[i] * half);
                ok = ok && corners3[i].z > 0.05;
                if (ok)
                    corners[i] = camera.project(corners3[i]);
                ok = ok && inner.contains(corners[i]);
                if (ok && horizontal)
                    ok = below ? corners[i].y() > camera.horizon + 8 : corners[i].y() < camera.horizon - 8;
            }
            placed = ok;
        }
    }

    Generated out;
    QPainterPath horizonPath(QPointF(sheet.left(), camera.horizon));
    horizonPath.lineTo(QPointF(sheet.right(), camera.horizon));
    out.ideal.append(horizonPath);
    QPainterPath square(corners[0]);
    for (int i = 1; i < 4; ++i)
        square.lineTo(corners[i]);
    square.closeSubpath();
    out.ideal.append(square);
    QPainterPath ellipse;
    for (int k = 0; k <= kEllipseSamples; ++k) {
        const double t = 2 * std::numbers::pi * k / kEllipseSamples;
        const QPointF p = camera.project(center3 + u * (half * std::cos(t)) + w * (half * std::sin(t)));
        if (k == 0)
            ellipse.moveTo(p);
        else
            ellipse.lineTo(p);
    }
    out.ideal.append(ellipse);

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionLine(painter, horizonPath.elementAt(0), horizonPath.elementAt(1));
    // Aristas: 0-1 y 3-2 van según u; 1-2 y 0-3 según w.
    const std::array<std::pair<int, int>, 4> edges{{{0, 1}, {3, 2}, {1, 2}, {0, 3}}};
    for (int axis = 0; axis < 2; ++axis) {
        const std::optional<QPointF> vp = camera.vanishing(axis == 0 ? u : w);
        if (!vp)
            continue;
        out.ideal.append(point(*vp));
        if (sheet.adjusted(10, 10, -10, -10).contains(*vp)) {
            appkit::guides::vanishingPoint(painter, *vp);
            continue;
        }
        // Afuera: las aristas de esa dirección, prolongadas hacia él desde su extremo más cercano.
        const int first = 2 * axis;
        for (int e = first; e < first + 2; ++e) {
            const QPointF a = corners[edges[e].first], b = corners[edges[e].second];
            const QPointF nearer = QLineF(a, *vp).length() < QLineF(b, *vp).length() ? a : b;
            const QPointF dir = (*vp - nearer) / QLineF(nearer, *vp).length();
            qreal length = 0.12 * sheet.width();
            while (!sheet.contains(nearer + dir * length) && length > 5)
                length *= 0.8;
            appkit::guides::constructionLine(painter, nearer, nearer + dir * length);
        }
    }
    appkit::guides::constructionLine(painter, corners[0], corners[2]);
    appkit::guides::constructionLine(painter, corners[1], corners[3]);
    for (const auto& [a, b] : edges)
        appkit::guides::guideLine(painter, corners[a], corners[b]);
    return out;
}

} // namespace ejercicios
