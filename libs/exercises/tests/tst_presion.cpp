#include <exercises/Presion.h>

#include <QLineF>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace ejercicios;

namespace {

const QRect kArea(0, 0, 1734, 1080);
constexpr double kPixelsPerMm = 5.3;
constexpr int N = Presion::kSamples;

SafeZone zoneFor(quint32 seed)
{
    SafeZone zone = SafeZone::withRandomOrientation(kArea, seed);
    zone.pixelsPerMm = kPixelsPerMm;
    return zone;
}

// Anchos de la banda a lo largo del recorrido: un lado va del arranque al final y el otro
// vuelve.
QList<double> widthsOf(const Generated& g)
{
    const QPainterPath& band = g.ideal[1];
    QList<double> widths;
    for (int i = 0; i <= N; ++i)
        widths.append(QLineF(QPointF(band.elementAt(i)), QPointF(band.elementAt(2 * N + 1 - i))).length());
    return widths;
}

enum class Kind { Ramp, Constant, Free };

Kind kindOf(const QList<double>& w)
{
    const double lo = *std::min_element(w.begin(), w.end()), hi = *std::max_element(w.begin(), w.end());
    if (hi - lo < 1e-6)
        return Kind::Constant;
    bool increasing = true;
    for (int i = 1; i < w.size(); ++i)
        increasing = increasing && w[i] > w[i - 1];
    return increasing ? Kind::Ramp : Kind::Free;
}

} // namespace

class TestPresion : public QObject {
    Q_OBJECT

private slots:
    // Cada perfil con su forma; anchos entre el mínimo y el máximo; el libre, suave y con
    // subidas y bajadas; la banda en la zona; salen los tres.
    void perfiles()
    {
        const Presion ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("width")] = 12.0;
        const double wMax = 12 * kPixelsPerMm, wMin = 0.2 * wMax;
        QSet<int> vistos;
        for (quint32 seed = 0; seed < 150; ++seed) {
            const SafeZone zone = zoneFor(seed);
            const Generated g = ej.generate(params, seed, zone);
            QCOMPARE(g.ideal.size(), 2);
            QVERIFY2(zone.contains(g.ideal[1]), qPrintable(QStringLiteral("semilla %1").arg(seed)));
            const QList<double> w = widthsOf(g);
            for (const double x : w)
                QVERIFY(x >= wMin - 1e-6 && x <= wMax + 1e-6);
            const Kind kind = kindOf(w);
            vistos.insert(int(kind));
            if (kind == Kind::Ramp) {
                QVERIFY(std::abs(w.first() - wMin) < 1e-6 && std::abs(w.last() - wMax) < 1e-6);
            } else if (kind == Kind::Constant) {
                QVERIFY(std::abs(w.first() - (wMin + wMax) / 2) < 1e-6);
            } else {
                int cambios = 0;
                double maxPaso = 0;
                for (int i = 1; i < w.size(); ++i)
                    maxPaso = std::max(maxPaso, std::abs(w[i] - w[i - 1]));
                for (int i = 2; i < w.size(); ++i)
                    cambios += (w[i] - w[i - 1]) * (w[i - 1] - w[i - 2]) < 0;
                QVERIFY(cambios >= 2);                    // sube y baja
                QVERIFY(maxPaso < 0.15 * (wMax - wMin)); // sin saltos
                QVERIFY(*std::max_element(w.begin(), w.end()) - *std::min_element(w.begin(), w.end()) > 0.3 * (wMax - wMin));
            }
        }
        QCOMPARE(vistos.size(), 3);
    }

    // Solo los perfiles habilitados.
    void soloLosHabilitados()
    {
        const Presion ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("profileRamp")] = false;
        params[QStringLiteral("profileFree")] = false;
        for (quint32 seed = 0; seed < 30; ++seed)
            QCOMPARE(int(kindOf(widthsOf(ej.generate(params, seed, zoneFor(seed))))), int(Kind::Constant));
    }

    // Curvo: el recorrido se aparta de la recta entre sus extremos.
    void trazoCurvo()
    {
        const Presion ej;
        QVariantMap params = ej.defaults();
        params[QStringLiteral("curved")] = true;
        const QPainterPath& c = ej.generate(params, 5, zoneFor(5)).ideal[0];
        const QLineF cuerda(QPointF(c.elementAt(0)), QPointF(c.elementAt(c.elementCount() - 1)));
        const QPointF medio = c.elementAt(N / 2);
        const QPointF d = cuerda.p2() - cuerda.p1();
        const double desvio = std::abs(d.x() * (cuerda.p1().y() - medio.y()) - d.y() * (cuerda.p1().x() - medio.x())) / cuerda.length();
        QVERIFY(desvio > 0.1 * cuerda.length());
    }

    void mismaSemillaMismoEjercicio()
    {
        const Presion ej;
        QCOMPARE(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal, ej.generate(ej.defaults(), 42, zoneFor(42)).ideal);
        QVERIFY(ej.generate(ej.defaults(), 42, zoneFor(42)).ideal != ej.generate(ej.defaults(), 43, zoneFor(42)).ideal);
    }
};

QTEST_MAIN(TestPresion)
#include "tst_presion.moc"
