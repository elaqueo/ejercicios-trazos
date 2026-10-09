#include <exercises/Renglon.h>

#include <QLineF>
#include <QTest>

#include <cmath>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

QLineF lineOf(const QPainterPath& p)
{
    return QLineF(QPointF(p.elementAt(0)), QPointF(p.elementAt(1)));
}

qreal distanceToLine(QPointF p, const QLineF& line)
{
    const QPointF d = line.p2() - line.p1();
    return std::abs(d.x() * (line.p1().y() - p.y()) - d.y() * (line.p1().x() - p.x())) / line.length();
}

// Ángulo sobre la hoja: positivo sube hacia la derecha.
qreal sheetDegrees(const QLineF& line)
{
    return std::atan2(-line.dy(), line.dx()) * 180 / 3.14159265358979323846;
}

SafeZone zoneFor(quint32 seed)
{
    SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
    zone.pixelsPerMm = 5.3;
    return zone;
}

} // namespace

class TestRenglon : public QObject {
    Q_OBJECT

private slots:
    // Largo en rango; base, altura de x e interlineado paralelos a las distancias pedidas en
    // mm, hacia arriba; de izquierda a derecha; todo en la zona; ángulo fijo respetado aunque
    // la zona esté girada.
    void medidasYAngulo()
    {
        const Renglon ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("spacing")] = 15.0;
        params[QStringLiteral("xHeight")] = 0.5;
        params[QStringLiteral("angle")] = 10;
        for (quint32 seed = 0; seed < 100; ++seed) {
            const SafeZone zone = zoneFor(seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 3);
            for (const QPainterPath& p : g.ideal)
                QVERIFY2(zone.contains(p), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            const QLineF base = lineOf(g.ideal[0]), x = lineOf(g.ideal[1]), techo = lineOf(g.ideal[2]);
            QVERIFY(std::abs(base.length() / (2 * zone.radius) - 0.70) < 1e-6);
            QVERIFY(std::abs(sheetDegrees(base) - 10) < 1e-6);
            QVERIFY(std::abs(sheetDegrees(x) - 10) < 1e-6 && std::abs(sheetDegrees(techo) - 10) < 1e-6);
            QVERIFY(std::abs(distanceToLine(x.p1(), base) - zone.mm(7.5)) < 1e-6);
            QVERIFY(std::abs(distanceToLine(techo.p1(), base) - zone.mm(15.0)) < 1e-6);
            QVERIFY(base.p2().x() > base.p1().x()); // de izquierda a derecha
            QVERIFY(techo.p1().y() < base.p1().y()); // las guías, arriba de la base
        }
    }

    // Al azar: dentro de ±20° y no siempre el mismo.
    void anguloAlAzar()
    {
        const Renglon ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("randomAngle")] = true;
        qreal minVisto = 90, maxVisto = -90;
        for (quint32 seed = 0; seed < 100; ++seed) {
            const qreal d = sheetDegrees(lineOf(ej.generate(params, seed, zoneFor(seed)).ideal[0]));
            QVERIFY(d >= -20 - 1e-6 && d <= 20 + 1e-6);
            minVisto = std::min(minVisto, d);
            maxVisto = std::max(maxVisto, d);
        }
        QVERIFY(minVisto < -15 && maxVisto > 15);
    }

    void mismaSemillaMismoRenglon()
    {
        const Renglon ej;
        QCOMPARE(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal, ej.generate(ej.defaults(), 42, zoneFor(42)).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal != ej.generate(ej.defaults(), 43, zoneFor(42)).ideal);
    }
};

QTEST_MAIN(TestRenglon)
#include "tst_renglon.moc"
