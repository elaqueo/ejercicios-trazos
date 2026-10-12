#include <exercises/CajasRotadas.h>

#include <QLineF>
#include <QTest>

#include <cmath>
#include <numbers>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

QPointF pointOf(const QPainterPath& p)
{
    return QPointF(p.elementAt(0));
}

} // namespace

class TestCajasRotadas : public QObject {
    Q_OBJECT

private slots:
    // Cantidad de cajas; los PF de cada una sobre el horizonte, con producto −f² respecto del
    // centro de visión y el ángulo de esa caja; ángulos en pasos de Δ; esquinas en la hoja y
    // del mismo lado del horizonte.
    void puntosDeFugaPorAngulo()
    {
        const CajasRotadas ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("count")] = 4;
        params[QStringLiteral("step")] = 20;
        params[QStringLiteral("eye")] = 0.6;
        const qreal f = 0.6 * 1734, cx = 1734 / 2.0;
        for (quint32 seed = 0; seed < 100; ++seed) {
            const Generated g = ej.generate(params, seed, SafeZone::fromRect(kArea));
            QCOMPARE(g.ideal.size(), 1 + 3 * 4);
            const qreal horizonte = QPointF(g.ideal[0].elementAt(0)).y();
            qreal anterior = -1;
            int lado = 0;
            for (int k = 0; k < 4; ++k) {
                const QPointF esquina = pointOf(g.ideal[1 + 3 * k]);
                const QPointF a = pointOf(g.ideal[2 + 3 * k]), b = pointOf(g.ideal[3 + 3 * k]);
                QCOMPARE(a.y(), horizonte);
                QCOMPARE(b.y(), horizonte);
                QVERIFY(std::abs((a.x() - cx) * (b.x() - cx) + f * f) < 1e-6 * f * f);
                const qreal angulo = std::atan((a.x() - cx) / f) * 180 / std::numbers::pi;
                QVERIFY(std::abs(angulo - std::round(angulo / 5) * 5) < 1e-6); // múltiplo de 5°
                QVERIFY(angulo >= 5 - 1e-6 && angulo <= 85 + 1e-6);
                if (anterior >= 0)
                    QVERIFY(std::abs(angulo - anterior - 20) < 1e-6); // pasos de Δ
                anterior = angulo;
                QVERIFY(QRectF(kArea).contains(esquina));
                const int este = esquina.y() > horizonte ? 1 : -1;
                QVERIFY(lado == 0 || lado == este);
                lado = este;
            }
        }
    }

    // Si el paso no entra en 80° de giro total, se achica.
    void giroTotalAcotado()
    {
        const CajasRotadas ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("count")] = 5;
        params[QStringLiteral("step")] = 30;
        const qreal f = 0.5 * 1734, cx = 1734 / 2.0;
        for (quint32 seed = 0; seed < 30; ++seed) {
            const Generated g = ej.generate(params, seed, SafeZone::fromRect(kArea));
            const qreal primero = std::atan((pointOf(g.ideal[2]).x() - cx) / f) * 180 / std::numbers::pi;
            const qreal ultimo = std::atan((pointOf(g.ideal[2 + 3 * 4]).x() - cx) / f) * 180 / std::numbers::pi;
            QVERIFY(ultimo - primero <= 80 + 1e-6);
            QVERIFY(ultimo <= 85 + 1e-6);
        }
    }

    void mismaSemillaMismoEjercicio()
    {
        const CajasRotadas ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestCajasRotadas)
#include "tst_cajas_rotadas.moc"
