#include "exercises/PrismaQueGira.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QPainter>
#include <QPen>
#include <QRandomGenerator>

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace ejercicios {

const QString PrismaQueGira::kAxis = QStringLiteral("axis");
const QString PrismaQueGira::kStep = QStringLiteral("step");
const QString PrismaQueGira::kPerspective = QStringLiteral("perspective");
const QString PrismaQueGira::kHidden = QStringLiteral("hidden");
const QString PrismaQueGira::kSize = QStringLiteral("size");

namespace prisma {

namespace {

struct Vec {
    double x, y, z;
};

double dot(Vec a, Vec b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec rotate(Vec p, Axis axis, double radians)
{
    const double c = std::cos(radians), s = std::sin(radians);
    switch (axis) {
    case Axis::Vertical: // sobre y: el frente va hacia la derecha
        return {c * p.x + s * p.z, p.y, -s * p.x + c * p.z};
    case Axis::Horizontal: // sobre x: el frente va hacia arriba
        return {p.x, c * p.y + s * p.z, -s * p.y + c * p.z};
    case Axis::Depth: // sobre z: antihorario en la pantalla
        return {c * p.x - s * p.y, s * p.x + c * p.y, p.z};
    }
    return p;
}

// La cámara inclinada: lo de arriba se acerca, así se ve la tapa.
Vec tilt(Vec p)
{
    const double e = kElevationDeg * std::numbers::pi / 180;
    const double c = std::cos(e), s = std::sin(e);
    return {p.x, c * p.y - s * p.z, s * p.y + c * p.z};
}

double sign(int corner, int bit)
{
    return (corner >> bit) & 1 ? 1.0 : -1.0;
}

} // namespace

std::array<Edge, kEdges> edges()
{
    std::array<Edge, kEdges> out{};
    int n = 0;
    for (int a = 0; a < kCorners; ++a)
        for (int bit = 0; bit < 3; ++bit)
            if (!((a >> bit) & 1))
                out[size_t(n++)] = {a, a | (1 << bit)};
    return out;
}

View project(const Box& box, Axis axis, double degrees, double eye)
{
    const double r = degrees * std::numbers::pi / 180;
    const Vec half{box.width / 2, box.height / 2, box.depth / 2};
    const auto place = [&](Vec p) { return tilt(rotate(p, axis, r)); };

    View view{};
    for (int i = 0; i < kCorners; ++i) {
        const Vec p = place({sign(i, 2) * half.x, sign(i, 1) * half.y, sign(i, 0) * half.z});
        const double f = eye / (eye - p.z);
        view.corners[size_t(i)] = QPointF(p.x * f, p.y * f);
    }
    // Caras: front[bit][lado]. La normal sale del centro de la cara; da la espalda si apunta
    // lejos de la cámara (en (0, 0, eye)).
    bool front[3][2];
    const double halves[3] = {half.z, half.y, half.x}; // por bit: 0 z, 1 y, 2 x
    for (int bit = 0; bit < 3; ++bit)
        for (int side = 0; side < 2; ++side) {
            const double s = side ? 1.0 : -1.0;
            const Vec n = place({bit == 2 ? s : 0, bit == 1 ? s : 0, bit == 0 ? s : 0});
            const Vec c{n.x * halves[bit], n.y * halves[bit], n.z * halves[bit]};
            front[bit][side] = dot(n, {-c.x, -c.y, eye - c.z}) > 0;
        }
    const auto all = edges();
    for (int k = 0; k < kEdges; ++k) {
        const Edge e = all[size_t(k)];
        const int along = std::countr_zero(unsigned(e.a ^ e.b));
        bool seen = false;
        for (int bit = 0; bit < 3; ++bit)
            if (bit != along)
                seen = seen || front[bit][(e.a >> bit) & 1];
        view.visible[size_t(k)] = seen;
    }
    return view;
}

Draw draw(quint32 seed, int axisParam)
{
    QRandomGenerator rng(seed);
    Draw out;
    // Proporciones de 1:1 a 1:3: una medida 1 y las otras dos entre 1 y 3, en cualquier orden.
    double d[3] = {1.0, 1.0 + 2.0 * rng.generateDouble(), 1.0 + 2.0 * rng.generateDouble()};
    for (int i = 2; i > 0; --i)
        std::swap(d[i], d[rng.bounded(i + 1)]);
    const double diagonal = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    out.box = {d[0] / diagonal, d[1] / diagonal, d[2] / diagonal};
    const int axis = axisParam == PrismaQueGira::kRandomAxis ? int(rng.bounded(3)) : std::clamp(axisParam, 0, 2);
    out.axis = Axis(axis);
    out.startDeg = 5 * int(rng.bounded(72)); // no siempre de frente
    return out;
}

} // namespace prisma

QString PrismaQueGira::id() const
{
    return QStringLiteral("prismaQueGira");
}

QString PrismaQueGira::title() const
{
    return QStringLiteral("Prisma que gira");
}

QString PrismaQueGira::group() const
{
    return QStringLiteral("Volumen");
}

QList<appkit::Param> PrismaQueGira::params() const
{
    using Type = appkit::Param::Type;
    return {
        sheetsParam(12), // media vuelta a 15°
        {.key = kAxis, .label = QStringLiteral("Eje (0 vertical · 1 horizontal · 2 profundidad · 3 al azar)"),
         .type = Type::Integer, .minimum = 0, .maximum = kRandomAxis, .step = 1, .defaultValue = 0},
        {.key = kStep, .label = QStringLiteral("Paso de giro"), .type = Type::Integer, .minimum = 5, .maximum = 45,
         .step = 5, .defaultValue = 15, .suffix = QStringLiteral("°")},
        {.key = kPerspective, .label = QStringLiteral("Perspectiva (0 casi isométrica · 1 fuerte)"),
         .type = Type::Real, .minimum = 0, .maximum = 1, .step = 0.05, .defaultValue = 0.5},
        {.key = kHidden, .label = QStringLiteral("Aristas ocultas (punteadas)"), .type = Type::Toggle,
         .defaultValue = true},
        {.key = kSize, .label = QStringLiteral("Tamaño (diagonal)"), .type = Type::Integer, .minimum = 60,
         .maximum = 180, .step = 10, .defaultValue = 120, .suffix = QStringLiteral(" mm")},
    };
}

StackGenerated PrismaQueGira::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const prisma::Draw d = prisma::draw(seed, v.value(kAxis).toInt());
    const int step = v.value(kStep).toInt();
    const bool hidden = v.value(kHidden).toBool();

    const QRectF sheet = zone.sheet.isValid() ? zone.sheet
                                              : QRectF(zone.center - QPointF(zone.radius, zone.radius),
                                                       QSizeF(2 * zone.radius, 2 * zone.radius));
    // La diagonal en mm; la cámara, de lejos (casi isométrica) a cerca (fuga fuerte).
    const double radius = v.value(kSize).toDouble() / 2;
    const double far = 1 - v.value(kPerspective).toDouble();
    const double eye = radius * (2.5 + 30 * far * far * far);
    // Que entre en la hoja con cualquier giro: lo más lejos que proyecta la esfera que lo envuelve.
    const double reach = zone.mm(radius * eye / std::sqrt(eye * eye - radius * radius));
    const double room = 0.42 * std::min(sheet.width(), sheet.height());
    const double scale = zone.mm(1) * std::min(1.0, room / reach);
    const QPointF center = sheet.center();
    const prisma::Box box{d.box.width * 2 * radius, d.box.height * 2 * radius, d.box.depth * 2 * radius};

    const QColor ink = appkit::theme::kMarcaHoja;
    QColor ghost = ink;
    ghost.setAlphaF(appkit::theme::kGhostOpacidad);
    QColor dashed = ink;
    dashed.setAlphaF(appkit::theme::kGhostOpacidad * 0.8f);
    const double width = std::max(1.0, zone.mm(0.3));
    QPen solid(ghost, width, Qt::SolidLine, Qt::RoundCap);
    QPen behind(dashed, width * 0.66, Qt::CustomDashLine, Qt::FlatCap);
    behind.setDashPattern({4, 4});
    QFont font(QString::fromLatin1(appkit::theme::kFuenteMono));
    font.setPixelSize(std::max(10, int(std::lround(zone.mm(3.5)))));
    const double margin = zone.mm(6);

    StackGenerated out;
    const auto edges = prisma::edges();
    for (int i = 0; i < sheetCount(v); ++i) {
        const int degrees = (d.startDeg + i * step) % 360;
        const prisma::View view = prisma::project(box, d.axis, degrees, eye);
        Generated g;
        QPainter painter(&g.guides);
        painter.setRenderHint(QPainter::Antialiasing);
        for (int k = 0; k < prisma::kEdges; ++k) {
            const QPointF a = view.corners[size_t(edges[size_t(k)].a)];
            const QPointF b = view.corners[size_t(edges[size_t(k)].b)];
            const QPointF pa = center + QPointF(a.x(), -a.y()) * scale;
            const QPointF pb = center + QPointF(b.x(), -b.y()) * scale;
            QPainterPath path(pa);
            path.lineTo(pb);
            g.ideal.append(path);
            if (view.visible[size_t(k)] || hidden) {
                painter.setPen(view.visible[size_t(k)] ? solid : behind);
                painter.drawLine(pa, pb);
            }
        }
        painter.setFont(font);
        painter.setPen(ink);
        painter.drawText(QRectF(sheet.left(), sheet.top(), sheet.width() - margin, sheet.height() - margin),
                         Qt::AlignRight | Qt::AlignBottom, QStringLiteral("%1°").arg(degrees));
        painter.end();
        out.sheets.append(g);
    }
    return out;
}

} // namespace ejercicios
