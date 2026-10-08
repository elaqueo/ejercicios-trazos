#include <appkit/ScreenChoice.h>

#include <QTest>

using appkit::ScreenId;

namespace {

const ScreenId kUltraWide{"GSM", "LG ULTRAWIDE", "148007", R"(\\.\DISPLAY1)"};
const ScreenId kW2243{"GSM", "W2243", "16843009", R"(\\.\DISPLAY2)"};

} // namespace

class TestScreenChoice : public QObject {
    Q_OBJECT

private slots:
    void eligeElGuardado()
    {
        QCOMPARE(appkit::findScreen({kUltraWide, kW2243}, kW2243), 1);
    }

    // Windows renumeró los monitores: el guardado se encuentra por modelo y serie.
    void sobreviveALaRenumeracion()
    {
        ScreenId ultraWide = kUltraWide;
        ScreenId w2243 = kW2243;
        ultraWide.name = R"(\\.\DISPLAY2)";
        w2243.name = R"(\\.\DISPLAY1)";
        QCOMPARE(appkit::findScreen({w2243, ultraWide}, kUltraWide), 1);
    }

    void desconectadoNoSeEncuentra()
    {
        QCOMPARE(appkit::findScreen({kUltraWide}, kW2243), std::nullopt);
    }

    // El guardado está desconectado y Windows le dio su nombre a otro monitor: no hay
    // que elegir ese otro (se abre en el principal), aunque coincida el nombre.
    void desconectadoNoSeConfundePorNombre()
    {
        ScreenId ultraWide = kUltraWide;
        ultraWide.name = kW2243.name;
        QCOMPARE(appkit::findScreen({ultraWide}, kW2243), std::nullopt);
    }

    // Dos monitores del mismo modelo se distinguen por la serie.
    void mismoModeloDistingueLaSerie()
    {
        ScreenId otro = kW2243;
        otro.serial = "999";
        otro.name = R"(\\.\DISPLAY3)";
        QCOMPARE(appkit::findScreen({kUltraWide, otro, kW2243}, kW2243), 2);
    }

    // Sin modelo ni serie (algunos adaptadores no los informan): por nombre de Windows.
    void sinDatosUsaElNombre()
    {
        const ScreenId generico{"", "", "", R"(\\.\DISPLAY2)"};
        QCOMPARE(appkit::findScreen({kUltraWide, generico}, generico), 1);
    }

    void idaYVueltaPorVariant()
    {
        const auto copia = ScreenId::fromVariant(kW2243.toVariant());
        QVERIFY(copia);
        QCOMPARE(copia->serial, kW2243.serial);
        QCOMPARE(copia->name, kW2243.name);
        QVERIFY(!ScreenId::fromVariant(QVariant()));
    }
};

QTEST_GUILESS_MAIN(TestScreenChoice)
#include "tst_screenchoice.moc"
