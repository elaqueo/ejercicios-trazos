#include <exercises/Concentricas.h>

#include <QLineF>
#include <QTest>

#include <cmath>
#include <numbers>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

// Una elipse de QPainterPath::addEllipse (transformada): arranca en el extremo del eje
// mayor y el cuarto punto sobre la curva (elemento 3) es el extremo del eje menor.
struct Ellipse {
    QPointF center;
    qreal a, b;
    qreal degrees() const { return std::asin(b / a) * 180 / std::numbers::pi; }
};

Ellipse ellipseOf(const QPainterPath& p)
{
    const QPointF c = p.boundingRect().center();
    return {c, QLineF(c, p.elementAt(0)).length(), QLineF(c, p.elementAt(3)).length()};
}

qreal distanceToLine(QPointF p, const QLineF& line)
{
    const QPointF d = line.p2() - line.p1();
    return std::abs(d.x() * (line.p1().y() - p.y()) - d.y() * (line.p1().x() - p.x())) / line.length();
}

} // namespace

class TestConcentricas : public QObject {
    Q_OBJECT

private slots:
    // Cantidad, grados en rango y en progresión, centros sobre el eje, mismo ancho, sin
    // superponerse y todo en la zona.
    void sobreElEjeEnProgresion()
    {
        const Concentricas ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("count")] = 4;
        params[QStringLiteral("degMin")] = 20;
        params[QStringLiteral("degMax")] = 70;
        int crecientes = 0;
        for (quint32 seed = 0; seed < 150; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 5);
            for (const QPainterPath& p : g.ideal)
                QVERIFY2(zone.contains(p), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            const QLineF eje(QPointF(g.ideal[0].elementAt(0)), QPointF(g.ideal[0].elementAt(1)));
            QList<Ellipse> e;
            for (int i = 1; i <= 4; ++i)
                e.append(ellipseOf(g.ideal[i]));
            for (int i = 0; i < 4; ++i) {
                QVERIFY(distanceToLine(e[i].center, eje) < 1e-6);
                QVERIFY(std::abs(e[i].a - e[0].a) < 1e-6);
                const qreal d = e[i].degrees();
                QVERIFY2(d >= 20 - 1e-6 && d <= 70 + 1e-6, qPrintable(QString::number(d)));
                QVERIFY(std::abs(d / 5 - std::round(d / 5)) < 1e-6); // múltiplo de 5°
            }
            const bool sube = e[1].degrees() > e[0].degrees();
            crecientes += sube;
            for (int i = 1; i < 4; ++i) {
                QVERIFY((e[i].degrees() > e[i - 1].degrees()) == sube); // progresión
                // Sin superponerse: la distancia entre centros alcanza los semiejes menores.
                QVERIFY(QLineF(e[i].center, e[i - 1].center).length() >= e[i].b + e[i - 1].b - 1e-6);
            }
            QVERIFY(std::abs(e.first().degrees() - (sube ? 20 : 70)) < 1e-6);
            QVERIFY(std::abs(e.last().degrees() - (sube ? 70 : 20)) < 1e-6);
        }
        QVERIFY(crecientes > 40 && crecientes < 110); // los dos sentidos
    }

    // En desorden: los mismos grados, no siempre en progresión.
    void enDesorden()
    {
        const Concentricas ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("count")] = 5;
        params[QStringLiteral("shuffle")] = true;
        int desordenados = 0;
        for (quint32 seed = 0; seed < 50; ++seed) {
            const Generated g = ej.generate(params, seed, SafeZone::fromRect(kArea));
            QList<int> grados;
            for (int i = 1; i <= 5; ++i)
                grados.append(int(std::lround(ellipseOf(g.ideal[i]).degrees())));
            QList<int> ordenados = grados;
            std::sort(ordenados.begin(), ordenados.end());
            QCOMPARE(ordenados, (QList<int>{15, 25, 40, 50, 60}));
            QList<int> alReves = ordenados;
            std::reverse(alReves.begin(), alReves.end());
            if (grados != ordenados && grados != alReves)
                ++desordenados;
        }
        QVERIFY(desordenados > 30);
    }

    void mismaSemillaMismoEjercicio()
    {
        const Concentricas ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestConcentricas)
#include "tst_concentricas.moc"
