#include <paintcore/CanvasWidget.h>

#include <QImage>
#include <QTest>

namespace {

// Trazo horizontal con mouse en y=50, de x=20 a x=180, y devuelve el lienzo.
QImage drawHorizontalStroke(paintcore::CanvasWidget& canvas)
{
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 50));
    for (int x = 50; x <= 180; x += 30)
        QTest::mouseMove(&canvas, QPoint(x, 50), 5);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(180, 50), 5);
    return canvas.grab().toImage();
}

// Cantidad de píxeles con tinta en la columna x.
int inkThickness(const QImage& image, int x)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        count += qGray(image.pixel(x, y)) < 200 ? 1 : 0;
    return count;
}

paintcore::BrushPreset presetFromJson(const char* json)
{
    paintcore::BrushPreset preset;
    preset.name = QStringLiteral("prueba");
    preset.json = json;
    return preset;
}

} // namespace

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

    void setBrushCambiaElTrazo()
    {
        paintcore::CanvasWidget fino;
        fino.resize(200, 100);
        fino.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fino));
        const int defaultThickness = inkThickness(drawHorizontalStroke(fino), 100);

        // Radio e^2.5 ≈ 12 px, opaco: un trazo mucho más ancho que el por defecto.
        paintcore::CanvasWidget grueso;
        grueso.resize(200, 100);
        grueso.show();
        QVERIFY(QTest::qWaitForWindowExposed(&grueso));
        grueso.setBrush(presetFromJson(R"({"version": 3, "settings": {)"
            R"("radius_logarithmic": {"base_value": 2.5, "inputs": {}}, "opaque": {"base_value": 1.0, "inputs": {}}}})"));
        const int wideThickness = inkThickness(drawHorizontalStroke(grueso), 100);

        QVERIFY2(wideThickness > 2 * defaultThickness,
                 qPrintable(QStringLiteral("por defecto %1 px, ancho %2 px").arg(defaultThickness).arg(wideThickness)));
    }

    // Un .myb rojo (color_s = color_v = 1) igual pinta con tinta negra: el color
    // sale gris neutro (r = g = b), más claro o más oscuro según la opacidad.
    void laTintaSiempreEsNegra()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(200, 100);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        canvas.setBrush(presetFromJson(R"({"version": 3, "settings": {)"
            R"("color_v": {"base_value": 1.0, "inputs": {}}, "color_s": {"base_value": 1.0, "inputs": {}},)"
            R"("radius_logarithmic": {"base_value": 2.0, "inputs": {}}, "opaque": {"base_value": 1.0, "inputs": {}}}})"));
        const QColor ink = drawHorizontalStroke(canvas).pixelColor(100, 50);
        QVERIFY2(ink.red() == ink.green() && ink.green() == ink.blue() && ink.red() < 200,
                 qPrintable(ink.name()));
    }

    // Regresión: con pinceles con suavizado (slow_tracking), un trazo nuevo no debe
    // unirse con una línea al final del anterior.
    void trazosSeparadosNoSeUnen()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(200, 100);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        canvas.setBrush(presetFromJson(R"({"version": 3, "settings": {)"
            R"("slow_tracking": {"base_value": 2.0, "inputs": {}},)"
            R"("radius_logarithmic": {"base_value": 1.5, "inputs": {}}, "opaque": {"base_value": 1.0, "inputs": {}}}})"));

        // Trazo 1 arriba, de izquierda a derecha; termina en (180, 15).
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 15));
        for (int x = 40; x <= 180; x += 20)
            QTest::mouseMove(&canvas, QPoint(x, 15), 10);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(180, 15), 10);

        // Trazo 2 abajo, empieza lejos, en (20, 85), un rato después.
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 85), 300);
        for (int x = 40; x <= 180; x += 20)
            QTest::mouseMove(&canvas, QPoint(x, 85), 10);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(180, 85), 10);

        // Entre los dos trazos (franja y = 35..65) no tiene que haber tinta.
        const QImage image = canvas.grab().toImage();
        for (int y = 35; y <= 65; ++y)
            for (int x = 0; x < image.width(); ++x)
                QVERIFY2(qGray(image.pixel(x, y)) > 240,
                         qPrintable(QStringLiteral("línea de unión en (%1, %2)").arg(x).arg(y)));
    }
};

QTEST_MAIN(TestCanvasWidget)
#include "tst_canvaswidget.moc"
