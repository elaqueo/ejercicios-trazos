#include <exercises/Recta.h>

#include <QLineF>
#include <QTest>

using namespace ejercicios;

class TestRecta : public QObject {
    Q_OBJECT

private slots:
    // La distancia entre los puntos cae entre distMin y distMax (fracción del
    // diámetro) y los dos extremos quedan en la zona, con cualquier orientación.
    void distanciaYZona()
    {
        const Recta ej;
        const QRect area(0, 0, 1734, 1080);
        QVariantMap params = ej.defaults();
        params[QStringLiteral("distMin")] = 0.3;
        params[QStringLiteral("distMax")] = 0.6;
        qreal minVisto = 1e9, maxVisto = 0;
        for (quint32 seed = 0; seed < 300; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(area, seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 1);
            const QPainterPath& p = g.ideal.first();
            QCOMPARE(p.elementCount(), 2);
            const qreal d = QLineF(QPointF(p.elementAt(0)), QPointF(p.elementAt(1))).length();
            const qreal diametro = 2 * zone.radius;
            QVERIFY2(d >= 0.3 * diametro - 1e-6 && d <= 0.6 * diametro + 1e-6,
                     qPrintable(QStringLiteral("semilla %1: distancia %2").arg(seed).arg(d / diametro)));
            QVERIFY(zone.contains(p));
            minVisto = qMin(minVisto, d / diametro);
            maxVisto = qMax(maxVisto, d / diametro);
        }
        // Usa todo el rango, no un valor fijo.
        QVERIFY(minVisto < 0.35 && maxVisto > 0.55);
    }

    void mismaSemillaMismaRecta()
    {
        const Recta ej;
        const SafeZone zone = SafeZone::fromRect(QRect(0, 0, 1734, 1080));
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }

    // No siempre pasa por el centro: el segmento se corre.
    void noSiemprePorElCentro()
    {
        const Recta ej;
        const SafeZone zone = SafeZone::fromRect(QRect(0, 0, 1734, 1080));
        int lejosDelCentro = 0;
        for (quint32 seed = 0; seed < 50; ++seed) {
            const QPainterPath& p = ej.generate(ej.defaults(), seed, zone).ideal.first();
            const QPointF medio = (QPointF(p.elementAt(0)) + QPointF(p.elementAt(1))) / 2;
            if (QLineF(medio, zone.center).length() > 100)
                ++lejosDelCentro;
        }
        QVERIFY(lejosDelCentro > 25);
    }
};

QTEST_MAIN(TestRecta)
#include "tst_recta.moc"
