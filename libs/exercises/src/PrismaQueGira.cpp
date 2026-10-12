#include "exercises/PrismaQueGira.h"

#include <QRandomGenerator>

#include <algorithm>
#include <bit>
#include <cmath>

namespace ejercicios {

namespace prisma {

namespace {

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
    using giro::Vec3;
    const Vec3 half{box.width / 2, box.height / 2, box.depth / 2};
    const auto place = [&](Vec3 p) { return giro::place(p, axis, degrees); };

    View view{};
    for (int i = 0; i < kCorners; ++i)
        view.corners[size_t(i)] = giro::project(place({sign(i, 2) * half.x, sign(i, 1) * half.y, sign(i, 0) * half.z}), eye);
    // Caras: front[bit][lado]. La normal sale del centro de la cara; da la espalda si apunta
    // lejos de la cámara.
    bool front[3][2];
    const double halves[3] = {half.z, half.y, half.x}; // por bit: 0 z, 1 y, 2 x
    for (int bit = 0; bit < 3; ++bit)
        for (int side = 0; side < 2; ++side) {
            const double s = side ? 1.0 : -1.0;
            const Vec3 n = place({bit == 2 ? s : 0, bit == 1 ? s : 0, bit == 0 ? s : 0});
            front[bit][side] = giro::facesCamera(n, {n.x * halves[bit], n.y * halves[bit], n.z * halves[bit]}, eye);
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
    out.axis = giro::axisFor(axisParam, rng);
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

QList<appkit::Param> PrismaQueGira::params() const
{
    return turnParams(0, QStringLiteral("Aristas ocultas (punteadas)"));
}

StackGenerated PrismaQueGira::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const prisma::Draw d = prisma::draw(seed, v.value(kAxis).toInt());
    const int step = v.value(kStep).toInt();
    const giro::Frame frame = giro::frame(v, zone);
    const double size = 2 * frame.radius; // la diagonal del prisma, en mm
    const prisma::Box box{d.box.width * size, d.box.height * size, d.box.depth * size};

    StackGenerated out;
    const auto edges = prisma::edges();
    for (int i = 0; i < sheetCount(v); ++i) {
        const int degrees = (d.startDeg + i * step) % 360;
        const prisma::View view = prisma::project(box, d.axis, degrees, frame.eye);
        Generated g;
        {
            giro::Ghost ghost(g, frame, zone, v.value(kHidden).toBool());
            for (int k = 0; k < prisma::kEdges; ++k)
                ghost.line(view.corners[size_t(edges[size_t(k)].a)], view.corners[size_t(edges[size_t(k)].b)],
                           view.visible[size_t(k)]);
            ghost.angle(degrees);
        }
        out.sheets.append(g);
    }
    return out;
}

} // namespace ejercicios
