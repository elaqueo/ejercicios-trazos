#include <exercises/Ghosting.h>

#include <QSet>
#include <QTest>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

// La forma por la cantidad de elementos de su trazado: recta 2, curva cuadrática (guardada
// como cúbica) 4, elipse 13.
QString shapeOf(const Generated& g)
{
    switch (g.ideal.first().elementCount()) {
    case 2: return QStringLiteral("recta");
    case 4: return QStringLiteral("curva");
    case 13: return QStringLiteral("elipse");
    default: return QStringLiteral("?");
    }
}

} // namespace

class TestGhosting : public QObject {
    Q_OBJECT

private slots:
    // El tiempo visible sale de la configuración; salen solo las formas habilitadas (y todas
    // ellas); la forma entra en la zona.
    void formasYTiempo()
    {
        const Ghosting ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("seconds")] = 4.5;
        QSet<QString> vistas;
        for (quint32 seed = 0; seed < 90; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.visibleMs, 4500);
            QCOMPARE(g.ideal.size(), 1);
            QVERIFY(!g.guides.isNull());
            QVERIFY(zone.contains(g.ideal.first()));
            vistas.insert(shapeOf(g));
        }
        QCOMPARE(vistas, (QSet<QString>{QStringLiteral("recta"), QStringLiteral("curva"), QStringLiteral("elipse")}));

        params[QStringLiteral("recta")] = false;
        params[QStringLiteral("curva")] = false;
        for (quint32 seed = 0; seed < 30; ++seed)
            QCOMPARE(shapeOf(ej.generate(params, seed, SafeZone::fromRect(kArea))), QStringLiteral("elipse"));
    }

    void mismaSemillaMismaForma()
    {
        const Ghosting ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestGhosting)
#include "tst_ghosting.moc"
