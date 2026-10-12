#include <exercises/Elipse.h>

#include <QLineF>
#include <QSet>
#include <QTest>

#include <cmath>
#include <numbers>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

QLineF lineOf(const QPainterPath& p)
{
    return QLineF(QPointF(p.elementAt(0)), QPointF(p.elementAt(1)));
}

// El grado que corresponde a los ejes: sen(grado) = menor / mayor.
qreal degreesOf(const Generated& g)
{
    return std::asin(lineOf(g.ideal[2]).length() / lineOf(g.ideal[1]).length()) * 180 / std::numbers::pi;
}

} // namespace

class TestElipse : public QObject {
    Q_OBJECT

private slots:
    // Solo grados habilitados (y todos ellos); eje mayor en rango; ejes perpendiculares que
    // se cortan en el centro; todo en la zona.
    void gradoTamanoYZona()
    {
        const Elipse ej;
        QVariantMap params = ej.defaults();
        params[Elipse::degreeKey(15)] = false;
        params[Elipse::degreeKey(60)] = false;
        QSet<int> vistos;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 3);
            for (const QPainterPath& p : g.ideal)
                QVERIFY2(zone.contains(p), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            const int grado = int(std::lround(degreesOf(g)));
            QVERIFY(std::abs(degreesOf(g) - grado) < 1e-6);
            QVERIFY2(grado == 30 || grado == 45 || grado == 75, qPrintable(QString::number(grado)));
            vistos.insert(grado);
            const QLineF mayor = lineOf(g.ideal[1]), menor = lineOf(g.ideal[2]);
            const qreal tamano = mayor.length() / (2 * zone.radius);
            QVERIFY2(tamano >= 0.25 - 1e-6 && tamano <= 0.70 + 1e-6, qPrintable(QString::number(tamano)));
            QVERIFY(std::abs(std::fmod(mayor.angleTo(menor), 180.0) - 90) < 1e-6);
            QVERIFY(QLineF(mayor.center(), menor.center()).length() < 1e-6);
            // La elipse pasa por los extremos de los ejes.
            QVERIFY(g.ideal[0].boundingRect().adjusted(-1, -1, 1, 1).contains(mayor.p1()));
        }
        QCOMPARE(vistos, (QSet<int>{30, 45, 75}));
    }

    // Sin ningún grado habilitado valen todos.
    void ningunoEsTodos()
    {
        const Elipse ej;
        QVariantMap params = ej.defaults();
        for (const int d : Elipse::kDegrees)
            params[Elipse::degreeKey(d)] = false;
        QSet<int> vistos;
        for (quint32 seed = 0; seed < 200; ++seed)
            vistos.insert(int(std::lround(degreesOf(ej.generate(params, seed, SafeZone::fromRect(kArea))))));
        QCOMPARE(vistos.size(), 5);
    }

    void mismaSemillaMismaElipse()
    {
        const Elipse ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestElipse)
#include "tst_elipse.moc"
