#include <exercises/Cajas.h>

#include <QLineF>
#include <QSet>
#include <QTest>

#include <cmath>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

QPointF pointOf(const QPainterPath& p)
{
    return QPointF(p.elementAt(0));
}

// Los PF: los puntos sueltos después de la esquina.
QList<QPointF> vanishingOf(const Generated& g)
{
    QList<QPointF> points;
    for (int i = 2; i < g.ideal.size(); ++i)
        if (g.ideal[i].elementCount() == 1)
            points.append(pointOf(g.ideal[i]));
    return points;
}

// Las rectas de ejemplo: los trazados de dos puntos después de la esquina.
QList<QLineF> examplesOf(const Generated& g)
{
    QList<QLineF> lines;
    for (int i = 2; i < g.ideal.size(); ++i)
        if (g.ideal[i].elementCount() == 2)
            lines.append(QLineF(QPointF(g.ideal[i].elementAt(0)), QPointF(g.ideal[i].elementAt(1))));
    return lines;
}

} // namespace

class TestCajas : public QObject {
    Q_OBJECT

private slots:
    // 1 o 2 PF según el modo (salen los dos); sobre el horizonte, que es horizontal; la
    // esquina en la hoja, fuera del horizonte y dentro del cono de visión.
    void horizonteYPuntosDeFuga()
    {
        const Cajas ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("separation")] = 0.8;
        int uno = 0, dos = 0;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            const QLineF horizonte(QPointF(g.ideal[0].elementAt(0)), QPointF(g.ideal[0].elementAt(1)));
            QCOMPARE(horizonte.dy(), 0.0);
            QCOMPARE(horizonte.length(), 1734.0); // de punta a punta de la hoja
            const QPointF esquina = pointOf(g.ideal[1]);
            QVERIFY(QRectF(kArea).contains(esquina));
            QVERIFY(std::abs(esquina.y() - horizonte.y1()) >= 0.08 * 1080 - 1e-6);
            const QList<QPointF> pfs = vanishingOf(g);
            const int pf = int(pfs.size());
            QVERIFY(pf == 1 || pf == 2);
            for (const QPointF& p : pfs)
                QCOMPARE(p.y(), horizonte.y1());
            if (pf == 2) {
                ++dos;
                const QPointF a = pfs[0], b = pfs[1];
                const qreal separacion = b.x() - a.x();
                QVERIFY(std::abs(separacion - 0.8 * 1734) < 1e-6);
                // Cono de visión: a no más de un tercio de la separación del punto medio.
                QVERIFY2(QLineF(esquina, (a + b) / 2).length() <= separacion / 3 + 1e-6,
                         qPrintable(QStringLiteral("semilla %1").arg(seed)));
            } else {
                ++uno;
            }
        }
        QVERIFY(uno > 50 && dos > 50);
    }

    // Solo el modo habilitado; sin ninguno, los dos.
    void modosHabilitados()
    {
        const Cajas ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("pf1")] = false;
        for (quint32 seed = 0; seed < 40; ++seed)
            QCOMPARE(vanishingOf(ej.generate(params, seed, SafeZone::fromRect(kArea))).size(), 2);
        params[QStringLiteral("pf2")] = false;
        QSet<qsizetype> tamanos;
        for (quint32 seed = 0; seed < 40; ++seed)
            tamanos.insert(vanishingOf(ej.generate(params, seed, SafeZone::fromRect(kArea))).size());
        QCOMPARE(tamanos, (QSet<qsizetype>{1, 2}));
    }

    // Con separación mayor que la hoja, los dos PF quedan afuera.
    void puntosDeFugaFueraDeLaHoja()
    {
        const Cajas ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("pf1")] = false;
        params[QStringLiteral("separation")] = 1.6;
        for (quint32 seed = 0; seed < 40; ++seed) {
            const Generated g = ej.generate(params, seed, SafeZone::fromRect(kArea));
            const QList<QPointF> pfs = vanishingOf(g);
            QVERIFY(pfs[0].x() < 0);
            QVERIFY(pfs[1].x() > 1734);
            // Dos rectas de ejemplo por PF: una arriba y otra abajo del horizonte, dentro de la
            // hoja, que prolongadas pasan por su PF.
            const QList<QLineF> ejemplos = examplesOf(g);
            QCOMPARE(ejemplos.size(), 4);
            const qreal horizonte = pfs[0].y();
            for (int i = 0; i < 4; ++i) {
                const QLineF& l = ejemplos[i];
                const QPointF pf = pfs[i / 2];
                QVERIFY(QRectF(kArea).adjusted(-1e-6, -1e-6, 1e-6, 1e-6).contains(l.p1()));
                QVERIFY(QRectF(kArea).adjusted(-1e-6, -1e-6, 1e-6, 1e-6).contains(l.p2()));
                const QPointF d = l.p2() - l.p1(), e = pf - l.p1();
                QVERIFY(std::abs(d.x() * e.y() - d.y() * e.x()) / std::hypot(e.x(), e.y()) < 1e-6 * std::hypot(d.x(), d.y()) + 1e-6);
                QVERIFY((l.p1().y() < horizonte) == (i % 2 == 0)); // primero arriba, después abajo
            }
        }
        // Con los PF adentro, no hay rectas de ejemplo.
        params[QStringLiteral("separation")] = 0.6;
        QVERIFY(examplesOf(ej.generate(params, 3, SafeZone::fromRect(kArea))).isEmpty());
    }

    void mismaSemillaMismaCaja()
    {
        const Cajas ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestCajas)
#include "tst_cajas.moc"
