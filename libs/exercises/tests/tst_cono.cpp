#include <exercises/ConoQueGira.h>

#include <QLineF>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace ejercicios;
using cono::Axis;
using cono::Shape;

namespace {

SafeZone hoja()
{
    SafeZone zone = SafeZone::fromRect(QRect(0, 0, 1734, 1080));
    zone.pixelsPerMm = 1734 / 325.0;
    return zone;
}

QByteArray bytes(const QPicture& picture)
{
    return QByteArray(picture.data(), int(picture.size()));
}

bool same(const QList<QPointF>& a, const QList<QPointF>& b, double tolerance = 1e-9)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i)
        if (QLineF(a[i], b[i]).length() > tolerance)
            return false;
    return true;
}

QList<QPointF> points(const Generated& g)
{
    QList<QPointF> out;
    for (const QPainterPath& path : g.ideal)
        for (int i = 0; i < path.elementCount(); ++i)
            out.append(QPointF(path.elementAt(i)));
    return out;
}

double cross(QPointF o, QPointF a, QPointF b)
{
    return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
}

} // namespace

class TestCono : public QObject {
    Q_OBJECT

private slots:
    // La silueta son dos generatrices que van del borde de la base al vértice y dejan todo el
    // cono de un lado, con cualquier eje y giro.
    void siluetaDesdeElVertice()
    {
        const Shape shape{2, 5};
        for (const Axis axis : {Axis::Vertical, Axis::Horizontal, Axis::Depth})
            for (int degrees = 0; degrees < 360; degrees += 25) {
                const cono::View view = cono::project(shape, axis, degrees, 15);
                for (const cono::Segment& s : view.silhouette) {
                    QCOMPARE(s.b, view.apex);
                    double lo = 0, hi = 0;
                    for (const QPointF& p : view.rim) {
                        const double c = cross(s.a, s.b, p);
                        lo = std::min(lo, c);
                        hi = std::max(hi, c);
                    }
                    QVERIFY(lo > -1e-3 || hi < 1e-3);
                }
                // Sin silueta, el vértice cae adentro de la base (visto de punta).
                if (view.silhouette.empty()) {
                    double lo = 0, hi = 0;
                    for (int i = 0; i + 1 < int(view.rim.size()); ++i) {
                        const double c = cross(view.rim[size_t(i)], view.rim[size_t(i) + 1], view.apex);
                        lo = std::min(lo, c);
                        hi = std::max(hi, c);
                    }
                    QVERIFY(lo > -1e-6 || hi < 1e-6);
                }
            }
    }

    // De pie y sin girar, desde un poco arriba, de la base solo se ve el tramo de adelante (la
    // base mira abajo); sobre el horizontal a 180° el vértice apunta abajo y la base, que mira
    // arriba, se ve entera. De punta (base hacia la cámara) no hay silueta.
    void ocultoLoDeAtras()
    {
        const Shape shape{2, 4};
        const cono::View up = cono::project(shape, Axis::Vertical, 0, 20);
        int hidden = 0;
        for (int i = 0; i < cono::kRimSteps; ++i) {
            const double mid = 2 * std::numbers::pi * (i + 0.5) / cono::kRimSteps;
            if (!up.rimVisible[size_t(i)])
                QVERIFY(std::sin(mid) < 0.5); // lo oculto, atrás
            hidden += !up.rimVisible[size_t(i)];
        }
        QVERIFY(hidden > cono::kRimSteps / 4 && hidden < cono::kRimSteps * 3 / 4);
        QVERIFY(up.seam.visible);
        QVERIFY(!up.mark.visible); // la marca está en la base, que mira abajo
        const cono::View down = cono::project(shape, Axis::Horizontal, 180, 20);
        QVERIFY(std::all_of(down.rimVisible.begin(), down.rimVisible.end(), [](bool v) { return v; }));
        // La base de frente a la cámara: sobre el horizontal a 90° + 22° (la inclinación); el
        // vértice cae adentro de la base.
        const cono::View end = cono::project(shape, Axis::Horizontal, 90 + giro::kElevationDeg, 20);
        QVERIFY(end.silhouette.empty());
        QVERIFY(std::all_of(end.rimVisible.begin(), end.rimVisible.end(), [](bool v) { return v; }));
    }

    // Cada hoja suma un paso; por defecto, 12 hojas sobre el horizontal, con costura (dos trazos).
    void cadaHojaSumaUnPaso()
    {
        const ConoQueGira ej;
        QCOMPARE(sheetCount(ej.defaults()), 12);
        QCOMPARE(ej.defaults().value(ConoQueGira::kAxis).toInt(), 1);
        QCOMPARE(ej.group(), QStringLiteral("Volumen"));
        for (int axis = 0; axis <= ConoQueGira::kRandomAxis; ++axis) {
            QVariantMap params = ej.defaults();
            params[ConoQueGira::kAxis] = axis;
            params[ConoQueGira::kStep] = 15;
            const StackGenerated fine = ej.generate(params, 42, hoja());
            params[ConoQueGira::kStep] = 30;
            const StackGenerated coarse = ej.generate(params, 42, hoja());
            for (int i = 0; i < 6; ++i)
                QVERIFY(same(points(coarse.sheets[i]), points(fine.sheets[2 * i])));
            QVERIFY(!same(points(fine.sheets[0]), points(fine.sheets[1])));
        }
        QVariantMap params = ej.defaults();
        const int with = int(ej.generate(params, 3, hoja()).sheets[0].ideal.size());
        params[ConoQueGira::kSeam] = false;
        QCOMPARE(int(ej.generate(params, 3, hoja()).sheets[0].ideal.size()), with - 2);
    }

    // La misma semilla da el mismo cono, otra otro; altura/diámetro de 1:3 a 3:1, en la esfera de
    // radio 1.
    void mismaSemillaMismoCono()
    {
        const ConoQueGira ej;
        QVariantMap params = ej.defaults();
        params[ConoQueGira::kAxis] = ConoQueGira::kRandomAxis;
        const StackGenerated a = ej.generate(params, 123, hoja());
        const StackGenerated b = ej.generate(params, 123, hoja());
        const StackGenerated c = ej.generate(params, 124, hoja());
        for (int i = 0; i < a.sheets.size(); ++i)
            QCOMPARE(bytes(a.sheets[i].guides), bytes(b.sheets[i].guides));
        QVERIFY(bytes(a.sheets[0].guides) != bytes(c.sheets[0].guides));
        QSet<int> axes;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const cono::Draw d = cono::draw(seed, ConoQueGira::kRandomAxis);
            const double ratio = d.shape.height / (2 * d.shape.radius);
            QVERIFY(ratio >= 1.0 / 3 - 1e-9 && ratio <= 3 + 1e-9);
            QVERIFY(std::abs(std::hypot(d.shape.radius, d.shape.height / 2) - 1) < 1e-9);
            axes.insert(int(d.axis));
        }
        QCOMPARE(axes.size(), 3);
    }

    void dentroDeLaHoja()
    {
        const ConoQueGira ej;
        const SafeZone zone = hoja();
        for (const double perspective : {0.0, 1.0}) {
            QVariantMap params = ej.defaults();
            params[ConoQueGira::kSize] = 180;
            params[ConoQueGira::kPerspective] = perspective;
            params[ConoQueGira::kAxis] = ConoQueGira::kRandomAxis;
            params[ConoQueGira::kStep] = 5;
            params[kSheetsParam] = kMaxSheets;
            for (quint32 seed = 0; seed < 30; ++seed)
                for (const Generated& g : ej.generate(params, seed, zone).sheets)
                    for (const QPointF& p : points(g))
                        QVERIFY(zone.sheet.contains(p));
        }
    }
};

QTEST_MAIN(TestCono)
#include "tst_cono.moc"
