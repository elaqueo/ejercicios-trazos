#include <exercises/Parrafo.h>
#include <exercises/WritingCurve.h>

#include <QLineF>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);
constexpr double kPixelsPerMm = 5.3;

QList<QPointF> pointsOf(const QPainterPath& p)
{
    QList<QPointF> points;
    for (int i = 0; i < p.elementCount(); ++i)
        points.append(QPointF(p.elementAt(i)));
    return points;
}

// Distancia mínima entre dos polilíneas (de punto a segmento, en los dos sentidos): si es
// positiva, no se cruzan.
double polylineDistance(const QList<QPointF>& a, const QList<QPointF>& b)
{
    const auto pointToSegment = [](QPointF p, QPointF s0, QPointF s1) {
        const QPointF d = s1 - s0;
        const double len2 = d.x() * d.x() + d.y() * d.y();
        const double t = len2 > 0 ? std::clamp(((p.x() - s0.x()) * d.x() + (p.y() - s0.y()) * d.y()) / len2, 0.0, 1.0) : 0;
        return QLineF(p, s0 + t * d).length();
    };
    double best = 1e18;
    for (const QPointF& p : a)
        for (int i = 1; i < b.size(); ++i)
            best = std::min(best, pointToSegment(p, b[i - 1], b[i]));
    return best;
}

SafeZone zoneFor(quint32 seed)
{
    SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
    zone.pixelsPerMm = kPixelsPerMm;
    return zone;
}

} // namespace

class TestParrafo : public QObject {
    Q_OBJECT

private slots:
    // Cantidad de renglones; bases consecutivas a un interlineado exacto y sin cruzarse;
    // ningún renglón pasa la curvatura máxima; de izquierda a derecha; todo en la zona.
    void renglonesParalelos()
    {
        const Parrafo ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("lines")] = 5;
        params[QStringLiteral("spacing")] = 10.0;
        params[QStringLiteral("minRadius")] = 50;
        const double spacing = 10 * kPixelsPerMm, minRadius = 50 * kPixelsPerMm;
        for (quint32 seed = 0; seed < 80; ++seed) {
            const SafeZone zone = zoneFor(seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 10); // base y altura de x por renglón
            for (const QPainterPath& p : g.ideal)
                QVERIFY2(zone.contains(p), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            QList<QList<QPointF>> bases;
            for (int k = 0; k < 5; ++k)
                bases.append(pointsOf(g.ideal[2 * k]));
            for (int k = 0; k < 5; ++k) {
                const QList<double> c = polylineCurvature(bases[k]);
                QVERIFY2(*std::max_element(c.begin(), c.end()) <= 1.02 / minRadius,
                         qPrintable(QStringLiteral("semilla %1, renglón %2").arg(seed).arg(k)));
                QVERIFY(bases[k].last().x() > bases[k].first().x());
            }
            for (int k = 1; k < 5; ++k) {
                for (int i = 0; i < bases[k].size(); ++i) // a un interlineado, punto a punto
                    QVERIFY(std::abs(QLineF(bases[k][i], bases[k - 1][i]).length() - spacing) < 1e-6);
                QVERIFY(polylineDistance(bases[k], bases[k - 1]) > 0.9 * spacing); // no se cruzan
                QVERIFY(bases[k].first().y() > bases[k - 1].first().y()); // cada uno, abajo del anterior
            }
        }
    }

    void cantidadDeRenglones()
    {
        const Parrafo ej;
        for (int lines = 2; lines <= 6; ++lines) {
            QVariantMap params = ej.defaults();
            params[QStringLiteral("lines")] = lines;
            QCOMPARE(ej.generate(params, 9, zoneFor(9)).ideal.size(), 2 * lines);
        }
    }

    void mismaSemillaMismoParrafo()
    {
        const Parrafo ej;
        QCOMPARE(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal, ej.generate(ej.defaults(), 42, zoneFor(42)).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal != ej.generate(ej.defaults(), 43, zoneFor(42)).ideal);
    }
};

QTEST_MAIN(TestParrafo)
#include "tst_parrafo.moc"
