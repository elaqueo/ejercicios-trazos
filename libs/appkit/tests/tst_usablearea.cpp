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
