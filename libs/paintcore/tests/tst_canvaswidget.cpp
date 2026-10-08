#include <paintcore/CanvasWidget.h>

#include <QImage>
#include <QTest>

// Prueba de ejemplo (HU-03): construir el lienzo crea un pincel de libmypaint, y
// el lienzo vacío se ve blanco.
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
};

QTEST_MAIN(TestCanvasWidget)
#include "tst_canvaswidget.moc"
