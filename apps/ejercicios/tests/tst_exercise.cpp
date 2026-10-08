#include <exercises/Exercise.h>

#include <appkit/Guides.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>
#include <QTest>

#include <numbers>

using namespace ejercicios;

namespace {

// Ejercicio mínimo solo para probar el contrato: un punto al azar en la zona.
class PuntoAlAzar : public Exercise {
public:
    QString id() const override { return QStringLiteral("punto"); }
    QString title() const override { return QStringLiteral("Un punto"); }
    QVariantMap defaults() const override { return {{QStringLiteral("dist"), 0.5}}; }

    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override
    {
        QRandomGenerator rng(seed);
        const qreal angle = rng.bounded(2 * std::numbers::pi);
        const qreal dist = params.value(QStringLiteral("dist"), defaults().value(QStringLiteral("dist"))).toDouble();
        const QPointF p = zone.pointAt(angle, dist * zone.radius);

        Generated out;
        QPainterPath path(p);
        out.ideal.append(path);
        QPainter painter(&out.guides);
        appkit::guides::targetPoint(painter, p);
        return out;
    }
};

const SafeZone kZona = SafeZone::fromRect(QRect(0, 0, 1734, 1080));

} // namespace

class TestExercise : public QObject {
    Q_OBJECT

private slots:
    void zonaSeguraInscripta()
    {
        QCOMPARE(kZona.center, QPointF(867, 540));
        QCOMPARE(kZona.radius, 540.0);
        // Justo en el borde cuenta como adentro (tolerancia al redondeo).
        QVERIFY(kZona.contains(kZona.pointAt(1.0, 540)));
        QVERIFY(!kZona.contains(kZona.pointAt(1.0, 541)));
        // Ángulo 0 apunta a la derecha; pi/2 hacia abajo (eje Y de pantalla).
        QCOMPARE(kZona.pointAt(0, 100), QPointF(967, 540));
        QCOMPARE(kZona.pointAt(std::numbers::pi / 2, 100).y(), 640.0);
    }

    // HU-18: la orientación sale de la semilla, cubre 360° y rota lo que se genera.
    void orientacionAleatoria()
    {
        const QRect area(0, 0, 1734, 1080);
        const SafeZone a = SafeZone::withRandomOrientation(area, 1);
        const SafeZone b = SafeZone::withRandomOrientation(area, 1);
        const SafeZone c = SafeZone::withRandomOrientation(area, 2);
        QCOMPARE(a.orientation, b.orientation);
        QVERIFY(a.orientation != c.orientation);
        QCOMPARE(a.center, kZona.center);
        QCOMPARE(a.radius, kZona.radius);

        // Cubre las cuatro mitades del círculo en pocas semillas.
        bool cuadrante[4] = {};
        for (quint32 seed = 0; seed < 64; ++seed) {
            const qreal o = SafeZone::withRandomOrientation(area, seed).orientation;
            QVERIFY(o >= 0 && o < 2 * std::numbers::pi);
            cuadrante[int(o / (std::numbers::pi / 2))] = true;
        }
        QVERIFY(cuadrante[0] && cuadrante[1] && cuadrante[2] && cuadrante[3]);

        // pointAt y toCanvas aplican la orientación: con pi/2, "a la derecha" es abajo.
        const SafeZone rotada = SafeZone::fromRect(area, std::numbers::pi / 2);
        QVERIFY(QLineF(rotada.pointAt(0, 100), QPointF(867, 640)).length() < 1e-9);
        QVERIFY(QLineF(rotada.toCanvas({100, 0}), QPointF(867, 640)).length() < 1e-9);
    }

    // HU-18: toda la geometría ideal queda en la zona, con cualquier orientación.
    void geometriaDentroDeLaZona()
    {
        const PuntoAlAzar ej;
        const QRect area(0, 0, 1734, 1080);
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(area, seed);
            const Generated g = ej.generate(ej.defaults(), seed, zone);
            for (const QPainterPath& path : g.ideal)
                QVERIFY2(zone.contains(path), qPrintable(QStringLiteral("semilla %1").arg(seed)));
        }

        // Y la verificación discrimina: una ruta que sale del círculo no pasa.
        QPainterPath afuera(kZona.center);
        afuera.lineTo(kZona.center + QPointF(kZona.radius + 2, 0));
        QVERIFY(!kZona.contains(afuera));
        // Una curva con puntos de control adentro pasa.
        QPainterPath curva(kZona.pointAt(0, 400));
        curva.quadTo(kZona.center, kZona.pointAt(std::numbers::pi, 400));
        QVERIFY(kZona.contains(curva));
    }

    // Misma semilla y parámetros → misma geometría; otra semilla → otra (RNF-08).
    void mismaSemillaMismaGeometria()
    {
        const PuntoAlAzar ej;
        const Generated a = ej.generate(ej.defaults(), 1234, kZona);
        const Generated b = ej.generate(ej.defaults(), 1234, kZona);
        const Generated c = ej.generate(ej.defaults(), 1235, kZona);
        QCOMPARE(a.ideal, b.ideal);
        QVERIFY(a.ideal != c.ideal);
        QCOMPARE(a.ideal.size(), 1);
        QVERIFY(kZona.contains(a.ideal.first().currentPosition()));
    }

    void parametrosCambianLaGeometria()
    {
        const PuntoAlAzar ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("dist")] = 0.25;
        const Generated lejos = ej.generate(ej.defaults(), 7, kZona);
        const Generated cerca = ej.generate(params, 7, kZona);
        QCOMPARE(QLineF(kZona.center, lejos.ideal.first().currentPosition()).length(), 270.0);
        QCOMPARE(QLineF(kZona.center, cerca.ideal.first().currentPosition()).length(), 135.0);
    }

    // Las guías se generan junto con la geometría y no están vacías.
    void guiasGeneradas()
    {
        const PuntoAlAzar ej;
        const Generated g = ej.generate(ej.defaults(), 1, kZona);
        QVERIFY(!g.guides.isNull());
        QVERIFY(g.guides.boundingRect().contains(g.ideal.first().currentPosition().toPoint()));
    }
};

QTEST_MAIN(TestExercise)
#include "tst_exercise.moc"
