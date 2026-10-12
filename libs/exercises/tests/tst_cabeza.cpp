#include <exercises/CabezaQueGira.h>

#include <QLineF>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace ejercicios;
using cabeza::Axis;
using cabeza::Shape;

namespace {

SafeZone hoja()
{
    SafeZone zone = SafeZone::fromRect(QRect(0, 0, 1734, 1080));
    zone.pixelsPerMm = 1734 / 325.0;
    return zone;
}

constexpr double kLejos = 1e9;
const double kElev = giro::kElevationDeg * std::numbers::pi / 180;

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

} // namespace

class TestCabeza : public QObject {
    Q_OBJECT

private slots:
    // El contorno es un círculo de radio R·ojo/√(ojo² − R²), y la línea media, las cejas y los
    // cortes quedan adentro (son de la esfera), con cualquier eje y giro.
    void todoAdentroDelContorno()
    {
        const Shape shape{};
        const double eye = 6;
        const double contour = eye / std::sqrt(eye * eye - 1);
        for (const Axis axis : {Axis::Vertical, Axis::Horizontal, Axis::Depth})
            for (int degrees = 0; degrees < 360; degrees += 30) {
                const cabeza::View view = cabeza::project(shape, axis, degrees, eye);
                for (const QPointF& p : view.outline.points)
                    QVERIFY(std::abs(std::hypot(p.x(), p.y()) - contour) < 1e-9);
                for (const cabeza::Curve* c : {&view.middle, &view.brow, &view.cuts[0], &view.cuts[1]})
                    for (const QPointF& p : c->points)
                        QVERIFY(std::hypot(p.x(), p.y()) <= contour + 1e-9);
            }
    }

    // De frente (0° sobre el vertical), la línea media visible es vertical por el centro, las
    // cejas son simétricas y la mandíbula también; se ve justo la mitad de adelante de la línea de
    // las cejas (de lejos).
    void deFrente()
    {
        const Shape shape{};
        const cabeza::View view = cabeza::project(shape, Axis::Vertical, 0, kLejos);
        for (const QPointF& p : view.middle.points)
            QVERIFY(std::abs(p.x()) < 1e-9);
        QVERIFY(std::abs(view.face.a.x()) < 1e-9 && std::abs(view.face.b.x()) < 1e-9);
        QVERIFY(view.face.visible);
        QVERIFY(std::abs(view.jaw[0].a.x() + view.jaw[1].a.x()) < 1e-9);
        QCOMPARE(view.jaw[0].visible, view.jaw[1].visible);
        int shown = 0;
        for (int i = 0; i < cabeza::kSteps; ++i) {
            const double mid = 2 * std::numbers::pi * (i + 0.5) / cabeza::kSteps;
            QCOMPARE(bool(view.brow.visible[size_t(i)]), std::cos(mid) > 0); // adelante
            shown += view.brow.visible[size_t(i)];
            // simétricas: el punto i y el espejado (−a) tienen la misma altura y x opuesta
            const QPointF p = view.brow.points[size_t(i)], q = view.brow.points[size_t(cabeza::kSteps - i)];
            QVERIFY(std::abs(p.x() + q.x()) < 1e-9 && std::abs(p.y() - q.y()) < 1e-9);
        }
        QCOMPARE(shown, cabeza::kSteps / 2);
        // De frente no se ve ningún corte lateral.
        QVERIFY(std::none_of(view.cuts[0].visible.begin(), view.cuts[0].visible.end(), [](bool v) { return v; }));
    }

    // A 90° sobre el vertical se ve de frente el corte de la izquierda: (de lejos) una elipse de
    // ancho 2·corte y alto 2·corte·cos e (la cámara inclinada), entera.
    void deCostado()
    {
        const Shape shape{};
        const cabeza::View view = cabeza::project(shape, Axis::Vertical, 90, kLejos);
        const cabeza::Curve& cut = view.cuts[0];
        QVERIFY(std::all_of(cut.visible.begin(), cut.visible.end(), [](bool v) { return v; }));
        QVERIFY(std::none_of(view.cuts[1].visible.begin(), view.cuts[1].visible.end(), [](bool v) { return v; }));
        double left = 1e9, right = -1e9, bottom = 1e9, top = -1e9;
        for (const QPointF& p : cut.points) {
            left = std::min(left, p.x());
            right = std::max(right, p.x());
            bottom = std::min(bottom, p.y());
            top = std::max(top, p.y());
        }
        QVERIFY(std::abs(right - left - 2 * shape.cut) < 1e-6);
        QVERIFY(std::abs(top - bottom - 2 * shape.cut * std::cos(kElev)) < 1e-3);
    }

    // Cada hoja suma un paso; por defecto, 12 hojas sobre el vertical.
    void cadaHojaSumaUnPaso()
    {
        const CabezaQueGira ej;
        QCOMPARE(sheetCount(ej.defaults()), 12);
        QCOMPARE(ej.defaults().value(CabezaQueGira::kAxis).toInt(), 0);
        QCOMPARE(ej.group(), QStringLiteral("Volumen"));
        for (int axis = 0; axis <= CabezaQueGira::kRandomAxis; ++axis) {
            QVariantMap params = ej.defaults();
            params[CabezaQueGira::kAxis] = axis;
            params[CabezaQueGira::kStep] = 15;
            const StackGenerated fine = ej.generate(params, 42, hoja());
            params[CabezaQueGira::kStep] = 30;
            const StackGenerated coarse = ej.generate(params, 42, hoja());
            for (int i = 0; i < 6; ++i)
                QVERIFY(same(points(coarse.sheets[i]), points(fine.sheets[2 * i])));
            QVERIFY(!same(points(fine.sheets[0]), points(fine.sheets[1])));
        }
    }

    // La misma semilla da la misma cabeza, otra otra; todo dentro de la esfera de radio 1.
    void mismaSemillaMismaCabeza()
    {
        const CabezaQueGira ej;
        QVariantMap params = ej.defaults();
        params[CabezaQueGira::kAxis] = CabezaQueGira::kRandomAxis;
        const StackGenerated a = ej.generate(params, 123, hoja());
        const StackGenerated b = ej.generate(params, 123, hoja());
        const StackGenerated c = ej.generate(params, 124, hoja());
        for (int i = 0; i < a.sheets.size(); ++i)
            QCOMPARE(bytes(a.sheets[i].guides), bytes(b.sheets[i].guides));
        QVERIFY(bytes(a.sheets[0].guides) != bytes(c.sheets[0].guides));
        QSet<int> axes;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const cabeza::Draw d = cabeza::draw(seed, CabezaQueGira::kRandomAxis);
            QVERIFY(d.shape.skull <= 1 + 1e-9);
            QVERIFY(std::hypot(d.shape.chinDrop, d.shape.chinForward) <= 1 + 1e-9);
            QVERIFY(d.shape.cut < d.shape.skull);
            axes.insert(int(d.axis));
        }
        QCOMPARE(axes.size(), 3);
    }

    void dentroDeLaHoja()
    {
        const CabezaQueGira ej;
        const SafeZone zone = hoja();
        for (const double perspective : {0.0, 1.0}) {
            QVariantMap params = ej.defaults();
            params[CabezaQueGira::kSize] = 180;
            params[CabezaQueGira::kPerspective] = perspective;
            params[CabezaQueGira::kAxis] = CabezaQueGira::kRandomAxis;
            params[CabezaQueGira::kStep] = 5;
            params[kSheetsParam] = kMaxSheets;
            for (quint32 seed = 0; seed < 30; ++seed)
                for (const Generated& g : ej.generate(params, seed, zone).sheets)
                    for (const QPointF& p : points(g))
                        QVERIFY(zone.sheet.contains(p));
        }
    }
};

QTEST_MAIN(TestCabeza)
#include "tst_cabeza.moc"
