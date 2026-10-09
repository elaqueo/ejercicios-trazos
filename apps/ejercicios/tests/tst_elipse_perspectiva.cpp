#include <exercises/ElipsePerspectiva.h>

#include <QLineF>
#include <QPolygonF>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

QList<QPointF> pointsOf(const QPainterPath& p)
{
    QList<QPointF> points;
    for (int i = 0; i < p.elementCount(); ++i)
        points.append(QPointF(p.elementAt(i)));
    return points;
}

// Distancia del punto a la recta (infinita) por a y b.
double distanceToLine(QPointF p, QPointF a, QPointF b)
{
    const QPointF d = b - a;
    return std::abs(d.x() * (a.y() - p.y()) - d.y() * (a.x() - p.x())) / std::hypot(d.x(), d.y());
}

double distanceToSegment(QPointF p, QPointF a, QPointF b)
{
    const QPointF d = b - a;
    const double t = std::clamp(((p.x() - a.x()) * d.x() + (p.y() - a.y()) * d.y()) / (d.x() * d.x() + d.y() * d.y()), 0.0, 1.0);
    return QLineF(p, a + t * d).length();
}

} // namespace

class TestElipsePerspectiva : public QObject {
    Q_OBJECT

private slots:
    // Las aristas fugan a los PF; la elipse toca los cuatro lados desde adentro; el cuadrado
    // está en la hoja y, en el plano horizontal, no cruza el horizonte. Salen los dos planos y
    // los dos modos.
    void cuadradoCorrecto()
    {
        const ElipsePerspectiva ej;
        int paredes = 0, pisos = 0, centrales = 0, girados = 0;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const Generated g = ej.generate(ej.defaults(), seed, SafeZone::fromRect(kArea));
            const qreal horizonte = QPointF(g.ideal[0].elementAt(0)).y();
            const QList<QPointF> c = pointsOf(g.ideal[1]).mid(0, 4);
            const QList<QPointF> elipse = pointsOf(g.ideal[2]);
            QList<QPointF> pfs;
            for (int i = 3; i < g.ideal.size(); ++i)
                pfs.append(QPointF(g.ideal[i].elementAt(0)));
            QVERIFY(!pfs.isEmpty() && pfs.size() <= 2);
            for (const QPointF& p : c)
                QVERIFY2(QRectF(kArea).contains(p), qPrintable(QStringLiteral("semilla %1").arg(seed)));

            // Cada PF: dos lados opuestos pasan por él.
            const QList<std::pair<int, int>> lados{{0, 1}, {3, 2}, {1, 2}, {0, 3}};
            for (const QPointF& pf : pfs) {
                int pasan = 0;
                for (const auto& [a, b] : lados)
                    pasan += distanceToLine(pf, c[a], c[b]) < 1e-6 * std::max(1.0, QLineF(c[a], pf).length());
                QVERIFY2(pasan == 2, qPrintable(QStringLiteral("semilla %1: %2 lados").arg(seed).arg(pasan)));
                QCOMPARE(pf.y(), horizonte); // sin inclinación: todos sobre el horizonte
            }

            // La elipse: adentro del cuadrado y tocando los cuatro lados.
            const QPolygonF cuadrado(c);
            for (const QPointF& p : elipse)
                QVERIFY(cuadrado.containsPoint(p, Qt::OddEvenFill) ||
                        std::min({distanceToSegment(p, c[0], c[1]), distanceToSegment(p, c[1], c[2]),
                                  distanceToSegment(p, c[2], c[3]), distanceToSegment(p, c[3], c[0])}) < 1e-6);
            for (const auto& [a, b] : lados) {
                double minimo = 1e9;
                for (const QPointF& p : elipse)
                    minimo = std::min(minimo, distanceToSegment(p, c[a], c[b]));
                QVERIFY(minimo < 1e-6);
            }

            // Pared: dos lados verticales. Piso o techo: ninguno, y no cruza el horizonte.
            const bool pared = std::abs(c[1].x() - c[2].x()) < 1e-6 || std::abs(c[0].x() - c[1].x()) < 1e-6;
            if (pared) {
                ++paredes;
            } else {
                ++pisos;
                const bool abajo = c[0].y() > horizonte;
                for (const QPointF& p : c)
                    QCOMPARE(p.y() > horizonte, abajo);
            }
            const bool central = pfs.size() == 1 && std::abs(pfs[0].x() - 1734 / 2.0) < 1e-6;
            centrales += central;
            girados += !central;
        }
        QVERIFY(paredes > 50 && pisos > 50);
        QVERIFY(centrales > 50 && girados > 50);
    }

    // Solo el plano habilitado.
    void soloPared()
    {
        const ElipsePerspectiva ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("floor")] = false;
        for (quint32 seed = 0; seed < 40; ++seed) {
            const QList<QPointF> c = pointsOf(ej.generate(params, seed, SafeZone::fromRect(kArea)).ideal[1]);
            QVERIFY(std::abs(c[1].x() - c[2].x()) < 1e-6);
        }
    }

    void mismaSemillaMismoEjercicio()
    {
        const ElipsePerspectiva ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestElipsePerspectiva)
#include "tst_elipse_perspectiva.moc"
