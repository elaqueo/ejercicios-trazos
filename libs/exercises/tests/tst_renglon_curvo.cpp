#include <exercises/RenglonCurvo.h>
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

// Distancia con signo de cada punto a la cuerda (primero → último).
QList<double> sideOfChord(const QList<QPointF>& points)
{
    const QPointF a = points.first(), d = points.last() - a;
    const double len = std::hypot(d.x(), d.y());
    QList<double> sides;
    for (const QPointF& p : points)
        sides.append((d.x() * (p.y() - a.y()) - d.y() * (p.x() - a.x())) / len);
    return sides;
}

bool isS(const QList<QPointF>& base)
{
    const QList<double> s = sideOfChord(base);
    const double lo = *std::min_element(s.begin(), s.end()), hi = *std::max_element(s.begin(), s.end());
    return lo < -1 && hi > 1;
}

SafeZone zoneFor(quint32 seed)
{
    SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
    zone.pixelsPerMm = kPixelsPerMm;
    return zone;
}

} // namespace

class TestRenglonCurvo : public QObject {
    Q_OBJECT

private slots:
    // La curvatura nunca supera 1/radio mínimo; las guías van paralelas a las distancias en
    // mm; de izquierda a derecha; todo en la zona; salen C y S.
    void curvaturaYGuias()
    {
        const RenglonCurvo ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("minRadius")] = 60;
        params[QStringLiteral("spacing")] = 10.0;
        params[QStringLiteral("xHeight")] = 0.5;
        const double minRadius = 60 * kPixelsPerMm;
        int eses = 0;
        for (quint32 seed = 0; seed < 150; ++seed) {
            const SafeZone zone = zoneFor(seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 3);
            for (const QPainterPath& p : g.ideal)
                QVERIFY2(zone.contains(p), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            const QList<QPointF> base = pointsOf(g.ideal[0]), x = pointsOf(g.ideal[1]), top = pointsOf(g.ideal[2]);
            const QList<double> k = polylineCurvature(base);
            const double kMax = *std::max_element(k.begin(), k.end());
            QVERIFY2(kMax <= 1.02 / minRadius,
                     qPrintable(QStringLiteral("semilla %1: radio %2 px").arg(seed).arg(1 / kMax)));
            for (int i = 0; i < base.size(); ++i) {
                QVERIFY(std::abs(QLineF(base[i], x[i]).length() - 5 * kPixelsPerMm) < 1e-6);
                QVERIFY(std::abs(QLineF(base[i], top[i]).length() - 10 * kPixelsPerMm) < 1e-6);
            }
            QVERIFY(base.last().x() > base.first().x()); // de izquierda a derecha
            QVERIFY(top.first().y() < base.first().y()); // las guías, arriba
            eses += isS(base);
        }
        QVERIFY(eses > 40 && eses < 110);
    }

    // Sin S, solo C.
    void soloC()
    {
        const RenglonCurvo ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("allowS")] = false;
        for (quint32 seed = 0; seed < 60; ++seed)
            QVERIFY(!isS(pointsOf(ej.generate(params, seed, zoneFor(seed)).ideal[0])));
    }

    // Un radio mínimo chico frente al interlineado se sube a 1,5 interlineados.
    void radioNoMenorQueElInterlineado()
    {
        const RenglonCurvo ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("minRadius")] = 40;
        params[QStringLiteral("spacing")] = 25.0; // 1,5 × 25 = 37,5 mm < 40: manda el radio
        params[QStringLiteral("allowS")] = false;
        for (quint32 seed = 0; seed < 60; ++seed) {
            const QList<double> k = polylineCurvature(pointsOf(ej.generate(params, seed, zoneFor(seed)).ideal[0]));
            QVERIFY(*std::max_element(k.begin(), k.end()) <= 1.02 / (40 * kPixelsPerMm));
        }
    }

    void mismaSemillaMismoRenglon()
    {
        const RenglonCurvo ej;
        QCOMPARE(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal, ej.generate(ej.defaults(), 42, zoneFor(42)).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal != ej.generate(ej.defaults(), 43, zoneFor(42)).ideal);
    }
};

QTEST_MAIN(TestRenglonCurvo)
#include "tst_renglon_curvo.moc"
