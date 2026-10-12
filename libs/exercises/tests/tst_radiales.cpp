#include <exercises/Radiales.h>

#include <appkit/Theme.h>

#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

// Ángulos de los puntos de partida vistos desde el centro, ordenados (grados).
QList<qreal> anglesOf(const Generated& g)
{
    QList<qreal> angles;
    for (const QPainterPath& p : g.ideal)
        angles.append(QLineF(QPointF(p.elementAt(1)), QPointF(p.elementAt(0))).angle());
    std::sort(angles.begin(), angles.end());
    return angles;
}

// El mayor hueco entre ángulos consecutivos, dando la vuelta.
qreal largestGap(const QList<qreal>& angles)
{
    qreal gap = 360 - angles.last() + angles.first();
    for (int i = 1; i < angles.size(); ++i)
        gap = std::max(gap, angles[i] - angles[i - 1]);
    return gap;
}

} // namespace

class TestRadiales : public QObject {
    Q_OBJECT

private slots:
    // La cantidad pedida, todos a la distancia pedida del mismo centro, y todo en la zona.
    void cantidadDistanciaYZona()
    {
        const Radiales ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("count")] = 11;
        params[QStringLiteral("dist")] = 0.4;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 11);
            const QPointF centro = g.ideal.first().elementAt(1);
            for (const QPainterPath& p : g.ideal) {
                QVERIFY(zone.contains(p));
                QCOMPARE(QPointF(p.elementAt(1)), centro);
                QVERIFY(std::abs(QLineF(p.elementAt(0), centro).length() - 0.4 * 2 * zone.radius) < 1e-6);
            }
            // Vuelta entera: nada se amontona ni deja un hueco grande.
            QVERIFY(largestGap(anglesOf(g)) < 360.0 / 11 * 1.41);
        }
    }

    // Solo un sector: los puntos cubren como mucho 270° (queda un hueco de 90° o más).
    void soloUnSector()
    {
        const Radiales ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("sector")] = true;
        int medios = 0;
        for (quint32 seed = 0; seed < 100; ++seed) {
            const Generated g = ej.generate(params, seed, SafeZone::withRandomOrientation(kArea, seed));
            const qreal hueco = largestGap(anglesOf(g));
            QVERIFY2(hueco >= 90 - 1e-6, qPrintable(QStringLiteral("semilla %1: hueco %2").arg(seed).arg(hueco)));
            if (hueco >= 180 - 1e-6)
                ++medios;
        }
        QVERIFY(medios > 20 && medios < 80); // a veces 180°, a veces 270°
    }

    // El centro va en ámbar (el destino) y los puntos de partida en azul.
    void centroEnAmbar()
    {
        const Radiales ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        const Generated g = ej.generate(ej.defaults(), 3, zone);
        QImage image(kArea.size(), QImage::Format_RGB32);
        image.fill(appkit::theme::kHoja);
        QPainter painter(&image);
        painter.drawPicture(0, 0, g.guides);
        painter.end();
        QCOMPARE(QColor(image.pixel(QPointF(g.ideal.first().elementAt(1)).toPoint())), appkit::theme::kEnfasis);
        QCOMPARE(QColor(image.pixel(QPointF(g.ideal.first().elementAt(0)).toPoint())), appkit::theme::kGuia);
    }

    void mismaSemillaMismoEjercicio()
    {
        const Radiales ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestRadiales)
#include "tst_radiales.moc"
