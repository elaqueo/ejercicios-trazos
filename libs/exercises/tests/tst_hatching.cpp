#include <exercises/Hatching.h>

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

// Ángulo entre dos rectas, en grados, entre 0 y 180.
qreal angleBetween(const QLineF& a, const QLineF& b)
{
    return std::fmod(a.angleTo(b) + 360.0, 180.0);
}

// Distancia del punto a la recta (infinita) que contiene la línea.
qreal distanceToLine(QPointF p, const QLineF& line)
{
    const QPointF d = line.p2() - line.p1();
    return std::abs(d.x() * (line.p1().y() - p.y()) - d.y() * (line.p1().x() - p.x())) / line.length();
}

SafeZone zoneFor(quint32 seed)
{
    SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
    zone.pixelsPerMm = 5.3; // la escala del ultrawide con la hoja = tableta
    return zone;
}

} // namespace

class TestHatching : public QObject {
    Q_OBJECT

private slots:
    // Contorno y muestras en la zona; la segunda muestra es paralela a la primera, a la
    // distancia del espaciado en mm.
    void muestrasParalelasAlEspaciado()
    {
        const Hatching ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("spacing")] = 6.0;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = zoneFor(seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 3);
            for (const QPainterPath& p : g.ideal)
                QVERIFY2(zone.contains(p), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            const QLineF a = lineOf(g.ideal[1]), b = lineOf(g.ideal[2]);
            const qreal paralelas = angleBetween(a, b);
            QVERIFY(paralelas < 0.01 || paralelas > 179.99);
            QVERIFY2(std::abs(distanceToLine(b.p1(), a) - zone.mm(6.0)) < 0.5,
                     qPrintable(QStringLiteral("semilla %1: %2 px").arg(seed).arg(distanceToLine(b.p1(), a))));
            QVERIFY(b.length() < a.length()); // la segunda es corta
        }
    }

    // Ángulo fijo: la muestra forma ese ángulo con el primer lado del rectángulo.
    void anguloFijo()
    {
        const Hatching ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("randomAngle")] = false;
        params[QStringLiteral("angle")] = 60;
        for (quint32 seed = 0; seed < 50; ++seed) {
            const Generated g = ej.generate(params, seed, zoneFor(seed));
            const QPainterPath& contorno = g.ideal[0];
            const QLineF lado(QPointF(contorno.elementAt(0)), QPointF(contorno.elementAt(1)));
            QVERIFY(std::abs(angleBetween(lado, lineOf(g.ideal[1])) - 120) < 0.01 // Qt mide antihorario
                    || std::abs(angleBetween(lado, lineOf(g.ideal[1])) - 60) < 0.01);
        }
    }

    // Al azar, en pasos de 15° y no siempre el mismo.
    void anguloAlAzarEnPasos()
    {
        const Hatching ej;
        QSet<int> vistos;
        for (quint32 seed = 0; seed < 100; ++seed) {
            const Generated g = ej.generate(ej.defaults(), seed, zoneFor(seed));
            const QPainterPath& contorno = g.ideal[0];
            const QLineF lado(QPointF(contorno.elementAt(0)), QPointF(contorno.elementAt(1)));
            const qreal angulo = angleBetween(lado, lineOf(g.ideal[1]));
            const qreal resto = std::fmod(angulo, 15.0);
            QVERIFY(resto < 0.01 || resto > 14.99);
            vistos.insert(int(std::lround(angulo)) % 180);
        }
        QVERIFY(vistos.size() >= 8);
    }

    void mismaSemillaMismoEjercicio()
    {
        const Hatching ej;
        QCOMPARE(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal, ej.generate(ej.defaults(), 42, zoneFor(42)).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal != ej.generate(ej.defaults(), 43, zoneFor(42)).ideal);
    }
};

QTEST_MAIN(TestHatching)
#include "tst_hatching.moc"
