#include <exercises/Curva.h>

#include <appkit/Theme.h>

#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QTest>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

// Extremos, punto de paso (t = 0,5) y largo de la cuerda de la curva ideal. QPainterPath
// guarda la cuadrática como cúbica: moveTo, curveTo y dos puntos de datos.
struct Shape {
    QPointF a, b, mid;
};

Shape shapeOf(const QPainterPath& p)
{
    const QPointF p0 = p.elementAt(0), p1 = p.elementAt(1), p2 = p.elementAt(2), p3 = p.elementAt(3);
    return {p0, p3, (p0 + 3 * p1 + 3 * p2 + p3) / 8};
}

// Distancia del punto a la recta que pasa por a y b.
qreal distanceToLine(QPointF p, QPointF a, QPointF b)
{
    const QPointF d = b - a;
    return std::abs(d.x() * (a.y() - p.y()) - d.y() * (a.x() - p.x())) / QLineF(a, b).length();
}

} // namespace

class TestCurva : public QObject {
    Q_OBJECT

private slots:
    // El punto de paso se aparta de la cuerda entre devMin y devMax del largo, y la curva
    // entera queda en la zona, con cualquier orientación.
    void desvioYZona()
    {
        const Curva ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("devMin")] = 0.2;
        params[QStringLiteral("devMax")] = 0.4;
        qreal minVisto = 1, maxVisto = 0;
        int izquierda = 0;
        for (quint32 seed = 0; seed < 300; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 1);
            const QPainterPath& p = g.ideal.first();
            QCOMPARE(p.elementCount(), 4);
            QVERIFY(zone.contains(p));
            const Shape s = shapeOf(p);
            const qreal largo = QLineF(s.a, s.b).length();
            const qreal desvio = distanceToLine(s.mid, s.a, s.b) / largo;
            QVERIFY2(desvio >= 0.2 - 1e-6 && desvio <= 0.4 + 1e-6,
                     qPrintable(QStringLiteral("semilla %1: desvío %2").arg(seed).arg(desvio)));
            minVisto = qMin(minVisto, desvio);
            maxVisto = qMax(maxVisto, desvio);
            // El punto de paso equidista de los extremos (está sobre la mediatriz).
            QVERIFY(std::abs(QLineF(s.mid, s.a).length() - QLineF(s.mid, s.b).length()) < 1.0);
            // Lado de la curva respecto de la cuerda a → b.
            const QPointF d = s.b - s.a, m = s.mid - s.a;
            if (d.x() * m.y() - d.y() * m.x() > 0)
                ++izquierda;
        }
        QVERIFY(minVisto < 0.25 && maxVisto > 0.35); // usa todo el rango
        QVERIFY(izquierda > 90 && izquierda < 210);  // hacia los dos lados
    }

    // Con los valores de fábrica el largo respeta distMin y distMax.
    void largoEnRango()
    {
        const Curva ej;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Shape s = shapeOf(ej.generate(ej.defaults(), seed, zone).ideal.first());
            const qreal largo = QLineF(s.a, s.b).length() / (2 * zone.radius);
            QVERIFY2(largo >= 0.30 - 1e-6 && largo <= 0.75 + 1e-6,
                     qPrintable(QStringLiteral("semilla %1: largo %2").arg(seed).arg(largo)));
        }
    }

    void mismaSemillaMismaCurva()
    {
        const Curva ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }

    // Guías: los extremos tienen centro relleno; el punto de paso es un anillo hueco.
    void extremosDistintosDelPuntoDePaso()
    {
        const Curva ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        const Generated g = ej.generate(ej.defaults(), 7, zone);
        QImage image(kArea.size(), QImage::Format_RGB32);
        image.fill(appkit::theme::kHoja);
        QPainter painter(&image);
        painter.drawPicture(0, 0, g.guides);
        painter.end();
        const Shape s = shapeOf(g.ideal.first());
        const auto esGuia = [&](QPointF p) { return QColor(image.pixel(p.toPoint())) != appkit::theme::kHoja; };
        QVERIFY(esGuia(s.a));
        QVERIFY(esGuia(s.b));
        QVERIFY(!esGuia(s.mid)); // hueco
        QVERIFY(esGuia(s.mid + QPointF(appkit::theme::kRadioPuntoPaso, 0)));
    }
};

QTEST_MAIN(TestCurva)
#include "tst_curva.moc"
