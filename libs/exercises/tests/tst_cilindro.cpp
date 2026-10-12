#include <exercises/CilindroQueGira.h>

#include <QLineF>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace ejercicios;
using cilindro::Axis;
using cilindro::Shape;

namespace {

// La hoja de animación (325 mm de ancho) en la pantalla de 1734 px.
SafeZone hoja()
{
    SafeZone zone = SafeZone::fromRect(QRect(0, 0, 1734, 1080));
    zone.pixelsPerMm = 1734 / 325.0;
    return zone;
}

constexpr double kLejos = 1e9; // ojo en el infinito: proyección paralela
const double kElev = giro::kElevationDeg * std::numbers::pi / 180;

QByteArray bytes(const QPicture& picture)
{
    return QByteArray(picture.data(), int(picture.size()));
}

QList<QPointF> sorted(QList<QPointF> points)
{
    std::sort(points.begin(), points.end(), [](QPointF a, QPointF b) {
        return std::abs(a.x() - b.x()) > 1e-9 ? a.x() < b.x() : a.y() < b.y();
    });
    return points;
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

QList<QPointF> rim(const cilindro::View& view, int cap)
{
    // Sin el último punto, que repite el primero.
    return QList<QPointF>(view.rims[cap].begin(), view.rims[cap].end() - 1);
}

// Las puntas de los trazos de una hoja (ideal: una ruta por trazo).
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

class TestCilindro : public QObject {
    Q_OBJECT

private slots:
    // De lejos, de pie y sin girar, la tapa es una elipse de ancho 2r y alto 2r·sen e (la cámara
    // inclinada e), centrada a la altura h·cos e.
    void tapaElipse()
    {
        const Shape shape{2, 4};
        const cilindro::View view = cilindro::project(shape, Axis::Vertical, 0, kLejos);
        QCOMPARE(int(view.rims[1].size()), cilindro::kRimSteps + 1);
        double left = 1e9, right = -1e9, bottom = 1e9, top = -1e9;
        for (const QPointF& p : view.rims[1]) {
            left = std::min(left, p.x());
            right = std::max(right, p.x());
            bottom = std::min(bottom, p.y());
            top = std::max(top, p.y());
        }
        QVERIFY(std::abs(right - left - 4) < 1e-6);
        QVERIFY(std::abs(top - bottom - 4 * std::sin(kElev)) < 1e-3); // muestreada: casi exacta
        QVERIFY(std::abs((top + bottom) / 2 - 2 * std::cos(kElev)) < 1e-3);
    }

    // La silueta son dos generatrices que dejan todo el cilindro de un lado (es el contorno), con
    // cualquier eje y giro; de punta a la cámara no hay silueta y la tapa de adelante se ve entera.
    void siluetaTangente()
    {
        const Shape shape{2, 5};
        for (const Axis axis : {Axis::Vertical, Axis::Horizontal, Axis::Depth})
            for (int degrees = 0; degrees < 360; degrees += 25) {
                const cilindro::View view = cilindro::project(shape, axis, degrees, 15);
                QCOMPARE(int(view.silhouette.size()), 2);
                for (const cilindro::Segment& s : view.silhouette) {
                    double lo = 0, hi = 0;
                    for (int cap = 0; cap < 2; ++cap)
                        for (const QPointF& p : view.rims[cap]) {
                            const double c = cross(s.a, s.b, p);
                            lo = std::min(lo, c);
                            hi = std::max(hi, c);
                        }
                    QVERIFY(lo > -1e-3 || hi < 1e-3);
                }
            }
        // Sobre el horizontal a 292° el eje apunta a la cámara (290° − 22° de la inclinación… en
        // 292°: cos(292° − 22°) = 0).
        const cilindro::View end = cilindro::project(shape, Axis::Horizontal, 292, 15);
        QVERIFY(end.silhouette.empty());
        int shown = 0;
        for (int cap = 0; cap < 2; ++cap)
            shown += int(std::count(end.rimVisible[cap].begin(), end.rimVisible[cap].end(), true));
        QVERIFY(shown >= cilindro::kRimSteps);
    }

    // De pie y sin girar, desde un poco arriba, la tapa de arriba se ve entera; de la de abajo se
    // ve el tramo de adelante: sen t > r / (ojo · cos e), un poco menos de la mitad.
    void ocultoLaMitadDeAtras()
    {
        const Shape shape{2, 4};
        const double eye = 20;
        const cilindro::View view = cilindro::project(shape, Axis::Vertical, 0, eye);
        QVERIFY(std::all_of(view.rimVisible[1].begin(), view.rimVisible[1].end(), [](bool v) { return v; }));
        int hidden = 0;
        for (int i = 0; i < cilindro::kRimSteps; ++i) {
            const double mid = 2 * std::numbers::pi * (i + 0.5) / cilindro::kRimSteps;
            QCOMPARE(bool(view.rimVisible[0][size_t(i)]), std::sin(mid) > 2 / (eye * std::cos(kElev)));
            hidden += !view.rimVisible[0][size_t(i)];
        }
        QVERIFY(hidden > cilindro::kRimSteps / 2 && hidden < cilindro::kRimSteps * 0.6);
        QVERIFY(view.seam.visible); // la costura, adelante
        QVERIFY(view.mark.visible);
    }

    // Girando sobre su propio eje, el cilindro se ve igual: solo se mueve la costura.
    void laCosturaDejaVerElGiro()
    {
        const Shape shape{2, 4};
        const cilindro::View a = cilindro::project(shape, Axis::Vertical, 0, 20);
        const cilindro::View b = cilindro::project(shape, Axis::Vertical, 90, 20);
        for (int cap = 0; cap < 2; ++cap)
            QVERIFY(same(sorted(rim(a, cap)), sorted(rim(b, cap)), 1e-6));
        QVERIFY(same(sorted({a.silhouette[0].a, a.silhouette[0].b, a.silhouette[1].a, a.silhouette[1].b}),
                     sorted({b.silhouette[0].a, b.silhouette[0].b, b.silhouette[1].a, b.silhouette[1].b}), 1e-6));
        QVERIFY(QLineF(a.seam.a, b.seam.a).length() > 0.5);
        // Sin costura, dos trazos menos.
        const CilindroQueGira ej;
        QVariantMap params = ej.defaults();
        QVERIFY(params.value(CilindroQueGira::kSeam).toBool());
        const int with = int(ej.generate(params, 3, hoja()).sheets[0].ideal.size());
        params[CilindroQueGira::kSeam] = false;
        QCOMPARE(int(ej.generate(params, 3, hoja()).sheets[0].ideal.size()), with - 2);
    }

    // Cada hoja suma un paso: la hoja 2i a 15° es la hoja i a 30°, y a 45° la hoja 9 vuelve a la
    // 1. Por defecto, 12 hojas sobre el eje horizontal.
    void cadaHojaSumaUnPaso()
    {
        const CilindroQueGira ej;
        QCOMPARE(sheetCount(ej.defaults()), 12);
        QCOMPARE(ej.defaults().value(CilindroQueGira::kAxis).toInt(), 1);
        QCOMPARE(ej.group(), QStringLiteral("Volumen"));
        for (int axis = 0; axis <= CilindroQueGira::kRandomAxis; ++axis) {
            QVariantMap params = ej.defaults();
            params[CilindroQueGira::kAxis] = axis;
            params[CilindroQueGira::kStep] = 15;
            const StackGenerated fine = ej.generate(params, 42, hoja());
            params[CilindroQueGira::kStep] = 30;
            const StackGenerated coarse = ej.generate(params, 42, hoja());
            for (int i = 0; i < 6; ++i)
                QVERIFY(same(points(coarse.sheets[i]), points(fine.sheets[2 * i])));
            QVERIFY(!same(points(fine.sheets[0]), points(fine.sheets[1])));
            params[CilindroQueGira::kStep] = 45;
            const StackGenerated round = ej.generate(params, 42, hoja());
            QVERIFY(same(points(round.sheets[8]), points(round.sheets[0]), 1e-6));
        }
    }

    // La misma semilla da el mismo cilindro, otra semilla otro; largo/diámetro de 1:3 a 3:1, en
    // una esfera de radio 1; el eje al azar sale de la semilla.
    void mismaSemillaMismoCilindro()
    {
        const CilindroQueGira ej;
        QVariantMap params = ej.defaults();
        params[CilindroQueGira::kAxis] = CilindroQueGira::kRandomAxis;
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
            const cilindro::Draw d = cilindro::draw(seed, CilindroQueGira::kRandomAxis);
            const double ratio = d.shape.length / (2 * d.shape.radius);
            QVERIFY(ratio >= 1.0 / 3 - 1e-9 && ratio <= 3 + 1e-9);
            QVERIFY(std::abs(std::hypot(d.shape.radius, d.shape.length / 2) - 1) < 1e-9);
            QCOMPARE(d.startDeg % 5, 0);
            axes.insert(int(d.axis));
            QCOMPARE(cilindro::draw(seed, 0).axis, Axis::Vertical);
        }
        QCOMPARE(axes.size(), 3);
    }

    // Todo queda dentro de la hoja, aun con el cilindro más grande y la perspectiva más fuerte.
    void dentroDeLaHoja()
    {
        const CilindroQueGira ej;
        const SafeZone zone = hoja();
        for (const double perspective : {0.0, 0.5, 1.0}) {
            QVariantMap params = ej.defaults();
            params[CilindroQueGira::kSize] = 180;
            params[CilindroQueGira::kPerspective] = perspective;
            params[CilindroQueGira::kAxis] = CilindroQueGira::kRandomAxis;
            params[CilindroQueGira::kStep] = 5;
            params[kSheetsParam] = kMaxSheets;
            for (quint32 seed = 0; seed < 30; ++seed)
                for (const Generated& g : ej.generate(params, seed, zone).sheets)
                    for (const QPointF& p : points(g))
                        QVERIFY(zone.sheet.contains(p));
        }
    }
};

QTEST_MAIN(TestCilindro)
#include "tst_cilindro.moc"
