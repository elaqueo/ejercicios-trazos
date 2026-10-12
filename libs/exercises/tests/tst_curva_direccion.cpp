#include <exercises/CurvaDireccion.h>
#include <exercises/Direccion.h>

#include <appkit/Theme.h>

#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QSet>
#include <QTest>

#include <cmath>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);

// Cuerda de la curva: de la partida a la llegada.
QLineF chordOf(const Generated& g)
{
    const QPainterPath& p = g.ideal.first();
    return QLineF(QPointF(p.elementAt(0)), QPointF(p.elementAt(p.elementCount() - 1)));
}

// Control de la cuadrática: QPainterPath la guarda como cúbica, con el primer control en
// a + 2/3·(control − a).
QPointF quadControl(const QPainterPath& p)
{
    const QPointF a(p.elementAt(0)), c1(p.elementAt(1));
    return a + 1.5 * (c1 - a);
}

qreal sheetDegrees(const QLineF& line)
{
    const qreal d = std::atan2(line.dy(), line.dx()) * 180 / 3.14159265358979323846;
    return d < 0 ? d + 360 : d;
}

qreal angularDistance(qreal a, qreal b)
{
    const qreal d = std::fmod(std::abs(a - b), 360.0);
    return std::min(d, 360 - d);
}

QImage guidesImage(const Generated& g)
{
    QImage image(kArea.size(), QImage::Format_RGB32);
    image.fill(appkit::theme::kHoja);
    QPainter painter(&image);
    painter.drawPicture(0, 0, g.guides);
    return image;
}

} // namespace

class TestCurvaDireccion : public QObject {
    Q_OBJECT

private slots:
    // En el grupo Curvas, aparte de la dirección forzada de las líneas.
    void enCurvas()
    {
        const CurvaDireccion ej;
        QCOMPARE(ej.group(), QStringLiteral("Curvas"));
        QVERIFY(ej.id() != Direccion().id());
    }

    // Solo salen las direcciones habilitadas (la cuerda, sobre la hoja aunque la zona esté
    // girada), y salen todas; la curva es curva y entra en la zona.
    void soloLasHabilitadas()
    {
        const CurvaDireccion ej;
        QVariantMap params = ej.defaults();
        for (int i : {0, 2, 4, 6})
            params[Direccion::directionKey(i)] = false;
        QSet<int> vistas;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            QVERIFY(zone.contains(g.ideal.first()));
            const QLineF chord = chordOf(g);
            const qreal deg = sheetDegrees(chord);
            const int index = int(std::lround(deg / 45)) % 8;
            QVERIFY2(angularDistance(deg, index * 45) < 1e-6, qPrintable(QStringLiteral("%1°").arg(deg)));
            QVERIFY(index % 2 == 1);
            vistas.insert(index);
            const QPointF control = quadControl(g.ideal.first());
            const qreal flecha = QLineF(chord.center(), control).length() / 2; // desvío del punto medio
            QVERIFY(flecha >= 0.10 * chord.length() - 1e-6 && flecha <= 0.35 * chord.length() + 1e-6);
        }
        QCOMPARE(vistas, (QSet<int>{1, 3, 5, 7}));
    }

    // Con variación, a lo sumo ±15° (aquí, solo →).
    void variacionAcotada()
    {
        const CurvaDireccion ej;
        QVariantMap params = ej.defaults();
        for (int i = 1; i < Direccion::kDirectionCount; ++i)
            params[Direccion::directionKey(i)] = false;
        params[QStringLiteral("jitter")] = true;
        qreal maxVisto = 0;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const qreal d = angularDistance(sheetDegrees(chordOf(ej.generate(params, seed, SafeZone::fromRect(kArea)))), 0);
            QVERIFY(d <= 15 + 1e-6);
            maxVisto = std::max(maxVisto, d);
        }
        QVERIFY(maxVisto > 10);
    }

    // La curva a lograr se ve: en el medio de la curva hay azul de construcción (cerca, por
    // el guionado). Y la flecha va del lado de afuera, apuntando a la llegada.
    void curvaTenueYFlechaAfuera()
    {
        const CurvaDireccion ej;
        for (quint32 seed = 0; seed < 20; ++seed) {
            const Generated g = ej.generate(ej.defaults(), seed, SafeZone::fromRect(kArea));
            const QImage image = guidesImage(g);
            const QPainterPath& curve = g.ideal.first();
            bool tenue = false;
            for (qreal t = 0.3; t <= 0.7 && !tenue; t += 0.01) {
                const QPoint p = curve.pointAtPercent(t).toPoint();
                const QColor c(image.pixel(p));
                tenue = c != appkit::theme::kHoja && c.blue() > c.red();
            }
            QVERIFY2(tenue, qPrintable(QStringLiteral("semilla %1").arg(seed)));

            const QLineF chord = chordOf(g);
            const QPointF u = (chord.p2() - chord.p1()) / chord.length();
            const QPointF mid = chord.center();
            const QPointF control = quadControl(curve);
            const QPointF out = (mid - control) / QLineF(mid, control).length(); // hacia afuera
            const QPointF from = mid - u * chord.length() / 4 + out * 24;
            const QPointF to = mid + u * chord.length() / 4 + out * 24;
            const auto ambar = [&](QPointF p) { return QColor(image.pixel(p.toPoint())) == appkit::theme::kEnfasis; };
            const QPointF n(-u.y(), u.x());
            QVERIFY2(ambar(from), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            QVERIFY(ambar(from + 3 * n)); // el punto relleno del arranque
            QVERIFY(!ambar(to + 3 * n));  // en la punta no hay punto
        }
    }

    void mismaSemillaMismoEjercicio()
    {
        const CurvaDireccion ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestCurvaDireccion)
#include "tst_curva_direccion.moc"
