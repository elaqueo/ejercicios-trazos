#include <appkit/ConfirmOverlay.h>

#include <QTest>

using appkit::ConfirmOverlay;

class TestConfirmOverlay : public QObject {
    Q_OBJECT

private slots:
    // HU-87: la tecla pedida confirma; cualquier otra cancela; los modificadores solos no
    // deciden nada; y con Ctrl la tecla tampoco confirma.
    void soloLaTeclaConfirma()
    {
        ConfirmOverlay overlay(nullptr);
        int confirmed = 0, cancelled = 0;
        overlay.onConfirm = [&] { ++confirmed; };
        overlay.onCancel = [&] { ++cancelled; };
        const auto ask = [&] {
            overlay.ask(QStringLiteral("¿Borrar la hoja 2?"), QStringLiteral("Tiene trazos."), Qt::Key_Delete,
                        QStringLiteral("Supr"), QStringLiteral("la borra"));
            overlay.show();
        };

        ask();
        QTest::keyClick(&overlay, Qt::Key_Shift); // solo: no decide
        QVERIFY(overlay.isVisible());
        QTest::keyClick(&overlay, Qt::Key_Delete);
        QCOMPARE(confirmed, 1);
        QVERIFY(!overlay.isVisible());

        ask();
        QTest::keyClick(&overlay, Qt::Key_Escape);
        ask();
        QTest::keyClick(&overlay, Qt::Key_A);
        ask();
        QTest::keyClick(&overlay, Qt::Key_Delete, Qt::ControlModifier);
        QCOMPARE(confirmed, 1);
        QCOMPARE(cancelled, 3);
    }
};

QTEST_MAIN(TestConfirmOverlay)
#include "tst_confirmoverlay.moc"
