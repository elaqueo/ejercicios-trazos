#include <exercises/PrismaQueGira.h>

#include <QLineF>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

using namespace ejercicios;
using prisma::Axis;
using prisma::Box;

namespace {

// La hoja de animación (325 mm de ancho) en la pantalla de 1734 px.
SafeZone hoja()
{
    SafeZone zone = SafeZone::fromRect(QRect(0, 0, 1734, 1080));
    zone.pixelsPerMm = 1734 / 325.0;
    return zone;
}

constexpr double kLejos = 1e9; // ojo en el infinito: proyección paralela

QByteArray bytes(const QPicture& picture)
{
    return QByteArray(picture.data(), int(picture.size()));
}

// Las esquinas como conjunto (ordenadas), para comparar vistas que las numeran distinto.
QList<QPointF> sorted(const prisma::View& view)
{
    QList<QPointF> out(view.corners.begin(), view.corners.end());
    std::sort(out.begin(), out.end(), [](QPointF a, QPointF b) {
        return std::abs(a.x() - b.x()) > 1e-9 ? a.x() < b.x() : a.y() < b.y();
    });
    return out;
}

bool same(const QList<QPointF>& a, const QList<QPointF>& b)
{
    for (int i = 0; i < a.size(); ++i)
        if (QLineF(a[i], b[i]).length() > 1e-9)
            return false;
    return true;
}

// Las aristas ocultas, como pares de esquinas.
QList<QPair<int, int>> hidden(const prisma::View& view)
{
    QList<QPair<int, int>> out;
    const auto edges = prisma::edges();
    for (int k = 0; k < prisma::kEdges; ++k)
        if (!view.visible[size_t(k)])
            out.append({edges[size_t(k)].a, edges[size_t(k)].b});
    return out;
}

// Las puntas de las aristas de una hoja (ideal: una ruta por arista).
QList<QPointF> points(const Generated& g)
{
    QList<QPointF> out;
    for (const QPainterPath& path : g.ideal)
        for (int i = 0; i < path.elementCount(); ++i)
            out.append(QPointF(path.elementAt(i)));
    return out;
}

} // namespace

class TestPrisma : public QObject {
    Q_OBJECT

private slots:
    // A 0° y de lejos, la esquina de adelante arriba a la derecha queda en (w/2, h/2·cos e −
    // d/2·sin e) (la cámara inclinada e); 12 aristas entre esquinas vecinas.
    void proyeccionDeFrente()
    {
        const Box box{4, 2, 3};
        const prisma::View view = prisma::project(box, Axis::Vertical, 0, kLejos);
        const double e = prisma::kElevationDeg * std::numbers::pi / 180;
        const QPointF p = view.corners[7]; // x+, y+, z+
        QVERIFY(std::abs(p.x() - 2) < 1e-6);
        QVERIFY(std::abs(p.y() - (1 * std::cos(e) - 1.5 * std::sin(e))) < 1e-6);
        const QPointF q = view.corners[0]; // x−, y−, z−
        QVERIFY(std::abs(q.x() + 2) < 1e-6);
        QVERIFY(std::abs(q.y() - (-1 * std::cos(e) + 1.5 * std::sin(e))) < 1e-6);
        for (const prisma::Edge& edge : prisma::edges())
            QCOMPARE(std::popcount(unsigned(edge.a ^ edge.b)), 1);
    }

    // A 90° sobre cada eje, el prisma es el mismo que sin girar con dos medidas cambiadas.
    void proyeccionA90Grados()
    {
        const Box box{4, 2, 3};
        const double eye = 20;
        QVERIFY(same(sorted(prisma::project(box, Axis::Vertical, 90, eye)),
                     sorted(prisma::project({3, 2, 4}, Axis::Vertical, 0, eye))));
        QVERIFY(same(sorted(prisma::project(box, Axis::Horizontal, 90, eye)),
                     sorted(prisma::project({4, 3, 2}, Axis::Vertical, 0, eye))));
        QVERIFY(same(sorted(prisma::project(box, Axis::Depth, 90, eye)),
                     sorted(prisma::project({2, 4, 3}, Axis::Vertical, 0, eye))));
        // A 360°, igual que a 0°.
        QVERIFY(same(sorted(prisma::project(box, Axis::Horizontal, 360, eye)),
                     sorted(prisma::project(box, Axis::Horizontal, 0, eye))));
    }

    // Con perspectiva, lo de adelante se ve más grande que lo de atrás.
    void perspectivaAgrandaLoDeAdelante()
    {
        const prisma::View view = prisma::project({4, 2, 3}, Axis::Vertical, 0, 10);
        QVERIFY(view.corners[7].x() > view.corners[6].x() + 0.1); // z+ contra z−
        const prisma::View far = prisma::project({4, 2, 3}, Axis::Vertical, 0, kLejos);
        QVERIFY(std::abs(far.corners[7].x() - far.corners[6].x()) < 1e-6);
    }

    // De frente (desde un poco arriba) se ven el frente y la tapa: ocultas, las 5 aristas que no
    // tocan ninguna de las dos. A 45° se ven tres caras: ocultas, las 3 de la esquina del fondo.
    void aristasOcultasLasDeAtras()
    {
        const Box box{4, 2, 3};
        const auto front = hidden(prisma::project(box, Axis::Vertical, 0, 20));
        QCOMPARE(front.size(), 5);
        for (const auto& [a, b] : front) {
            QVERIFY(!((a & 1) && (b & 1)));   // ninguna en el frente (z+)
            QVERIFY(!((a & 2) && (b & 2)));   // ni en la tapa (y+)
        }
        const auto turned = hidden(prisma::project(box, Axis::Vertical, 45, 20));
        QCOMPARE(turned.size(), 3);
        for (const auto& [a, b] : turned)
            QVERIFY(a == 4 || b == 4); // la esquina del fondo: x+, y−, z−
        // Sin aristas ocultas en el dibujo, la geometría ideal igual tiene las 12.
        const PrismaQueGira ej;
        QVariantMap params = ej.defaults();
        params[PrismaQueGira::kHidden] = false;
        const StackGenerated g = ej.generate(params, 7, hoja());
        QCOMPARE(g.sheets[0].ideal.size(), prisma::kEdges);
    }

    // Cada hoja suma un paso: la hoja 2i a 15° es la hoja i a 30°, y a 45° la hoja 9 vuelve a
    // la 1 (360°). Hojas por defecto: 12.
    void cadaHojaSumaUnPaso()
    {
        const PrismaQueGira ej;
        QCOMPARE(sheetCount(ej.defaults()), 12);
        for (int axis = 0; axis <= PrismaQueGira::kRandomAxis; ++axis) {
            QVariantMap params = ej.defaults();
            params[PrismaQueGira::kAxis] = axis;
            params[kSheetsParam] = 12;
            params[PrismaQueGira::kStep] = 15;
            const StackGenerated fine = ej.generate(params, 42, hoja());
            params[PrismaQueGira::kStep] = 30;
            const StackGenerated coarse = ej.generate(params, 42, hoja());
            QCOMPARE(fine.sheets.size(), 12);
            for (int i = 0; i < 6; ++i)
                QVERIFY(same(points(coarse.sheets[i]), points(fine.sheets[2 * i])));
            QVERIFY(!same(points(fine.sheets[0]), points(fine.sheets[1])));
            params[PrismaQueGira::kStep] = 45;
            const StackGenerated round = ej.generate(params, 42, hoja());
            QVERIFY(same(points(round.sheets[8]), points(round.sheets[0])));
        }
    }

    // La misma semilla da el mismo prisma (guías y geometría), otra semilla otro; las medidas
    // van de 1:1 a 1:3, con diagonal 1, y el eje al azar sale de la semilla.
    void mismaSemillaMismoPrisma()
    {
        const PrismaQueGira ej;
        QVariantMap params = ej.defaults();
        params[PrismaQueGira::kAxis] = PrismaQueGira::kRandomAxis;
        const StackGenerated a = ej.generate(params, 123, hoja());
        const StackGenerated b = ej.generate(params, 123, hoja());
        const StackGenerated c = ej.generate(params, 124, hoja());
        for (int i = 0; i < a.sheets.size(); ++i) {
            QCOMPARE(bytes(a.sheets[i].guides), bytes(b.sheets[i].guides));
            QCOMPARE(a.sheets[i].ideal, b.sheets[i].ideal);
        }
        QVERIFY(bytes(a.sheets[0].guides) != bytes(c.sheets[0].guides));
        QSet<int> axes;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const prisma::Draw d = prisma::draw(seed, PrismaQueGira::kRandomAxis);
            const double m[3] = {d.box.width, d.box.height, d.box.depth};
            const double lo = *std::min_element(m, m + 3), hi = *std::max_element(m, m + 3);
            QVERIFY(hi / lo <= 3 + 1e-9);
            QVERIFY(std::abs(std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]) - 1) < 1e-9);
            QCOMPARE(d.startDeg % 5, 0);
            axes.insert(int(d.axis));
            QCOMPARE(prisma::draw(seed, 1).axis, Axis::Horizontal);
        }
        QCOMPARE(axes.size(), 3);
    }

    // Todo queda dentro de la hoja, aun con el prisma más grande y la perspectiva más fuerte.
    void dentroDeLaHoja()
    {
        const PrismaQueGira ej;
        const SafeZone zone = hoja();
        for (const double perspective : {0.0, 0.5, 1.0}) {
            QVariantMap params = ej.defaults();
            params[PrismaQueGira::kSize] = 180;
            params[PrismaQueGira::kPerspective] = perspective;
            params[PrismaQueGira::kAxis] = PrismaQueGira::kRandomAxis;
            params[PrismaQueGira::kStep] = 5;
            params[kSheetsParam] = kMaxSheets;
            for (quint32 seed = 0; seed < 30; ++seed)
                for (const Generated& g : ej.generate(params, seed, zone).sheets)
                    for (const QPointF& p : points(g))
                        QVERIFY(zone.sheet.contains(p));
        }
    }
};

QTEST_MAIN(TestPrisma)
#include "tst_prisma.moc"
