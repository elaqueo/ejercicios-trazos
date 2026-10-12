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

QLineF lineOf(const Generated& g)
{
    const QPainterPath& p = g.ideal.first();
    return QLineF(QPointF(p.elementAt(0)), QPointF(p.elementAt(1)));
}

// Dirección del trazo sobre la hoja, en grados con y hacia abajo (0 = →, 90 = ↓).
qreal sheetDegrees(const QLineF& line)
{
    const qreal d = std::atan2(line.dy(), line.dx()) * 180 / 3.14159265358979323846;
    return d < 0 ? d + 360 : d;
}

// Diferencia angular más corta, en grados.
qreal angularDistance(qreal a, qreal b)
{
    const qreal d = std::fmod(std::abs(a - b), 360.0);
    return std::min(d, 360 - d);
}

} // namespace

class TestDireccion : public QObject {
    Q_OBJECT

private slots:
    // Solo salen las direcciones habilitadas, y salen todas; sobre la hoja, aunque la zona
    // esté girada. Largo en rango y todo en la zona.
    void soloLasHabilitadas()
    {
        const Direccion ej;
        QVariantMap params = ej.defaults();
        for (int i : {0, 2, 4, 6}) // apaga →, ↓, ←, ↑: quedan las diagonales
            params[Direccion::directionKey(i)] = false;
        QSet<int> vistas;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
            const Generated g = ej.generate(params, seed, zone);
            QVERIFY(zone.contains(g.ideal.first()));
            const QLineF line = lineOf(g);
            const qreal deg = sheetDegrees(line);
            const int index = int(std::lround(deg / 45)) % 8;
            QVERIFY2(angularDistance(deg, index * 45) < 1e-6, qPrintable(QStringLiteral("%1°").arg(deg)));
            QVERIFY(index % 2 == 1);
            vistas.insert(index);
            const qreal largo = line.length() / (2 * zone.radius);
            QVERIFY(largo >= 0.25 - 1e-6 && largo <= 0.70 + 1e-6);
        }
        QCOMPARE(vistas, (QSet<int>{1, 3, 5, 7}));
    }

    // Sin ninguna habilitada valen todas.
    void ningunaEsTodas()
    {
        const Direccion ej;
        QVariantMap params = ej.defaults();
        for (int i = 0; i < Direccion::kDirectionCount; ++i)
            params[Direccion::directionKey(i)] = false;
        QSet<int> vistas;
        for (quint32 seed = 0; seed < 200; ++seed)
            vistas.insert(int(std::lround(sheetDegrees(lineOf(ej.generate(params, seed, SafeZone::fromRect(kArea)))) / 45)) % 8);
        QCOMPARE(vistas.size(), 8);
    }

    // Con variación, a lo sumo ±15° de la dirección elegida (aquí, solo →).
    void variacionAcotada()
    {
        const Direccion ej;
        QVariantMap params = ej.defaults();
        for (int i = 1; i < Direccion::kDirectionCount; ++i)
            params[Direccion::directionKey(i)] = false;
        params[QStringLiteral("jitter")] = true;
        qreal maxVisto = 0;
        for (quint32 seed = 0; seed < 200; ++seed) {
            const qreal d = angularDistance(sheetDegrees(lineOf(ej.generate(params, seed, SafeZone::fromRect(kArea)))), 0);
            QVERIFY(d <= 15 + 1e-6);
            maxVisto = std::max(maxVisto, d);
        }
        QVERIFY(maxVisto > 10);
    }

    // La flecha apunta de la partida a la llegada: el punto de arranque (relleno) queda del
    // lado de la partida y la punta del de la llegada.
    void flechaEnElSentido()
    {
        const Direccion ej;
        for (quint32 seed = 0; seed < 20; ++seed) {
            const Generated g = ej.generate(ej.defaults(), seed, SafeZone::fromRect(kArea));
            QImage image(kArea.size(), QImage::Format_RGB32);
            image.fill(appkit::theme::kHoja);
            QPainter painter(&image);
            painter.drawPicture(0, 0, g.guides);
            painter.end();
            const QLineF line = lineOf(g);
            const QPointF u = (line.p2() - line.p1()) / line.length();
            const QPointF n(-u.y(), u.x());
            const auto ambar = [&](QPointF p) { return QColor(image.pixel(p.toPoint())) == appkit::theme::kEnfasis; };
            const QPointF mid = line.center();
            // La flecha va a 24 px a un lado u otro; el arranque, a un cuarto antes del medio.
            bool encontrada = false;
            for (const qreal s : {1.0, -1.0}) {
                const QPointF from = mid - u * line.length() / 4 + s * 24 * n;
                const QPointF to = mid + u * line.length() / 4 + s * 24 * n;
                if (ambar(from)) {
                    encontrada = true;
                    QVERIFY(ambar(from + 3 * n)); // el punto relleno del arranque (radio 5)
                    QVERIFY(!ambar(to + 3 * n));  // en la punta no hay punto
                }
            }
            QVERIFY2(encontrada, qPrintable(QStringLiteral("semilla %1").arg(seed)));
        }
    }

    void mismaSemillaMismoEjercicio()
    {
        const Direccion ej;
        const SafeZone zone = SafeZone::fromRect(kArea);
        QCOMPARE(ej.generate(ej.defaults(), 42, zone).ideal, ej.generate(ej.defaults(), 42, zone).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zone).ideal != ej.generate(ej.defaults(), 43, zone).ideal);
    }
};

QTEST_MAIN(TestDireccion)
#include "tst_direccion.moc"
