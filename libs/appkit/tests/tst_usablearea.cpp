#include <appkit/Config.h>
#include <appkit/UsableArea.h>

#include <QTemporaryDir>
#include <QTest>

using appkit::ScreenId;

namespace {

const ScreenId kUltraWide{"on", "LG ULTRAWIDE", "148007", "LG ULTRAWIDE"};
const ScreenId kW2243{"on", "W2243", "16843009", "W2243"};

} // namespace

class TestUsableArea : public QObject {
    Q_OBJECT

private slots:
    void rectanguloDesdeDosEsquinas()
    {
        QCOMPARE(appkit::rectFromCorners({100, 50}, {900.4, 650.6}), QRect(QPoint(100, 50), QPoint(901, 651)));
        // En cualquier orden.
        QCOMPARE(appkit::rectFromCorners({900, 650}, {100, 50}), appkit::rectFromCorners({100, 50}, {900, 650}));
    }

    // Con la escala de Windows en 125 %, Qt ve 1536 × 864 donde hay 1920 × 1080 píxeles.
    void escalaDePantalla()
    {
        QCOMPARE(appkit::scaledRect(QRect(0, 0, 1536, 864), 1.25), QRect(0, 0, 1920, 1080));
        QCOMPARE(appkit::scaledRect(QRect(10, 9, 1519, 846), 1.25), QRect(13, 11, 1898, 1058));
        QCOMPARE(appkit::scaledRect(appkit::scaledRect(QRect(13, 11, 1898, 1058), 1 / 1.25), 1.25), QRect(13, 11, 1898, 1058));
        QCOMPARE(appkit::scaledRect(QRect(5, 6, 70, 80), 1.0), QRect(5, 6, 70, 80));
    }

    // El panel lateral queda dentro del área útil (pegado a su borde derecho), no
    // contra el borde de la ventana, que puede estar fuera del alcance del lápiz.
    void panelLateralDentroDelArea()
    {
        const QRect area(0, 0, 1734, 1080); // tableta mapeada a la izquierda del UltraWide
        const QRect panel = appkit::sidePanelRect(area, 440, 16);
        QCOMPARE(panel, QRect(1734 - 16 - 440, 16, 440, 1080 - 32));
        QVERIFY(area.contains(panel));

        // Área angosta: el panel se achica para entrar.
        const QRect angosta(100, 50, 300, 400);
        QVERIFY(angosta.contains(appkit::sidePanelRect(angosta, 440, 16)));
    }

    void seGuardaPorMonitor()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        {
            appkit::Config config("ejercicios", path);
            appkit::saveUsableArea(config, kUltraWide, QRect(400, 100, 1600, 900));
            appkit::saveUsableArea(config, kW2243, QRect(0, 0, 1920, 1080));
            // Recalibrar reemplaza, no duplica.
            appkit::saveUsableArea(config, kUltraWide, QRect(420, 90, 1500, 880));
        }
        appkit::Config config("ejercicios", path);
        QCOMPARE(appkit::loadUsableArea(config, kUltraWide), QRect(420, 90, 1500, 880));
        QCOMPARE(appkit::loadUsableArea(config, kW2243), QRect(0, 0, 1920, 1080));
        QCOMPARE(config.value("usableAreas", {}, appkit::Config::Scope::Common).toList().size(), 2);
    }

    void sinCalibrarNoHayArea()
    {
        QTemporaryDir dir;
        appkit::Config config("ejercicios", dir.filePath("config.json"));
        QVERIFY(!appkit::loadUsableArea(config, kUltraWide));
    }
};

QTEST_GUILESS_MAIN(TestUsableArea)
#include "tst_usablearea.moc"
