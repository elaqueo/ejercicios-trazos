#include <exercises/Exercise.h>

#include <appkit/Guides.h>

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
        // 1 px de margen: justo en el borde depende del redondeo de coma flotante.
        QVERIFY(kZona.contains(kZona.pointAt(1.0, 539)));
        QVERIFY(!kZona.contains(kZona.pointAt(1.0, 541)));
        // Ángulo 0 apunta a la derecha; pi/2 hacia abajo (eje Y de pantalla).
        QCOMPARE(kZona.pointAt(0, 100), QPointF(967, 540));
        QCOMPARE(kZona.pointAt(std::numbers::pi / 2, 100).y(), 640.0);
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
