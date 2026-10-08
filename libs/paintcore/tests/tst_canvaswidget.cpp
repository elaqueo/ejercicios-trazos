#include <paintcore/CanvasWidget.h>

#include <QImage>
#include <QTest>

class TestCanvasWidget : public QObject {
    Q_OBJECT

private slots:
    void lienzoVacioEsBlanco()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(64, 48);

        const QImage image = canvas.grab().toImage();

        QCOMPARE(image.size(), QSize(64, 48));
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::white));
        QCOMPARE(image.pixelColor(63, 47), QColor(Qt::white));
    }

    // Un trazo horizontal con mouse (presión fija) deja tinta continua a lo
    // largo de toda la línea, sin huecos entre muestras, y nada lejos de ella.
    // Con presión 0,5 el pincel por defecto deja tinta al ~50 % (gris ~130).
    void trazoConMouseEsContinuo()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(200, 100);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));

        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 50));
        // Muestras espaciadas 30 px, como un trazo rápido: libmypaint interpola.
        for (int x = 50; x <= 180; x += 30)
            QTest::mouseMove(&canvas, QPoint(x, 50), 5);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(180, 50), 5);

        const QImage image = canvas.grab().toImage();
        for (int x = 25; x <= 175; ++x)
            QVERIFY2(qGray(image.pixel(x, 50)) < 200,
                     qPrintable(QStringLiteral("hueco en x=%1 (gris %2)").arg(x).arg(qGray(image.pixel(x, 50)))));
        QCOMPARE(image.pixelColor(100, 10), QColor(Qt::white));
        QCOMPARE(image.pixelColor(100, 90), QColor(Qt::white));
    }
};

QTEST_MAIN(TestCanvasWidget)
#include "tst_canvaswidget.moc"
