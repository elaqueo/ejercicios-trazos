#include <paintcore/CanvasWidget.h>

#include <QImage>
#include <QPointingDevice>
#include <QTabletEvent>
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

        // Además es regresión de fix_uninitialized_brush.patch: sin él, ~1 de cada 70
        // corridas el primer trazo de un pincel nuevo no se pintaba (ancho 0 px).
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

    // Con la vista rotada, el trazo aparece en pantalla exactamente donde se hizo.
    void conVistaRotadaElTrazoQuedaBajoLaPunta()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(200, 200);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        canvas.setViewRotation(30);

        // Trazo horizontal en pantalla, de (40, 60) a (160, 60).
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 60));
        for (int x = 60; x <= 160; x += 20)
            QTest::mouseMove(&canvas, QPoint(x, 60), 5);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(160, 60), 5);

        const QImage image = canvas.grab().toImage();
        for (int x = 50; x <= 150; x += 10)
            QVERIFY2(qGray(image.pixel(x, 60)) < 200, qPrintable(QStringLiteral("sin tinta en (%1, 60)").arg(x)));
        QCOMPARE(image.pixelColor(100, 120), QColor(Qt::white));
    }

    // Shift + arrastre rota (con snap de 15°) sin pintar; la tecla 5 vuelve a 0°.
    void gestoDeRotacionYReset()
    {
        paintcore::CanvasWidget canvas;
        canvas.resize(200, 200);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));

        // De la derecha del centro (100, 100) hacia abajo: un cuarto de vuelta horario,
        // con un pequeño desvío que el snap absorbe.
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::ShiftModifier, QPoint(180, 100));
        QTest::mouseMove(&canvas, QPoint(160, 160), 5);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::ShiftModifier, QPoint(103, 180), 5);
        QCOMPARE(canvas.viewRotation(), 90.0);

        const QImage image = canvas.grab().toImage();
        QCOMPARE(image.pixelColor(170, 130), QColor(Qt::white)); // no pintó

        QTest::keyClick(&canvas, Qt::Key_5);
        QCOMPARE(canvas.viewRotation(), 0.0);
    }

    // viewrotation: un pincel caligráfico (dab elíptico con ángulo fijo) tiene que
    // verse igual en pantalla con la vista rotada, porque la pluma sigue la pantalla.
    // Pluma horizontal y vista a 45°: bien hecho, la línea horizontal sale fina; sin
    // viewrotation la pluma quedaría a 45° y con el signo invertido, a 90° (gruesa).
    void laPlumaCaligraficaSigueLaPantalla()
    {
        const char* pluma = R"({"version": 3, "settings": {)"
            R"("radius_logarithmic": {"base_value": 2.3, "inputs": {}}, "opaque": {"base_value": 1.0, "inputs": {}},)"
            R"("elliptical_dab_ratio": {"base_value": 8.0, "inputs": {}},)"
            R"("elliptical_dab_angle": {"base_value": 0.0, "inputs": {}}}})";
        auto grosorEnPantalla = [&](double rotation) {
            paintcore::CanvasWidget canvas;
            canvas.resize(200, 200);
            canvas.show();
            if (!QTest::qWaitForWindowExposed(&canvas))
                return -1;
            canvas.setViewRotation(rotation);
            canvas.setBrush(presetFromJson(pluma));
            QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 100));
            for (int x = 50; x <= 170; x += 20)
                QTest::mouseMove(&canvas, QPoint(x, 100), 5);
            QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(170, 100), 5);
            return inkThickness(canvas.grab().toImage(), 100);
        };
        const int sinRotar = grosorEnPantalla(0);
        const int rotada = grosorEnPantalla(45);
        QVERIFY2(qAbs(sinRotar - rotada) <= 2,
                 qPrintable(QStringLiteral("sin rotar %1 px, rotada 45° %2 px").arg(sinRotar).arg(rotada)));
    }

    // Windows Ink entrega los timestamps con resolución de ~15,6 ms, así que llegan de
    // a pares (o más) con el mismo valor. El mismo trazo con timestamps agrupados tiene
    // que verse igual que con timestamps parejos.
    void timestampsAgrupadosPintanIgualQueParejos()
    {
        const char* segunVelocidad = R"({"version": 3, "settings": {)"
            R"("radius_logarithmic": {"base_value": 1.2, "inputs": {"speed1": [[0.0, 0.0], [4.0, 1.5]]}},)"
            R"("opaque": {"base_value": 1.0, "inputs": {}}}})";
        QPointingDevice stylus(QStringLiteral("lápiz de prueba"), 1, QInputDevice::DeviceType::Stylus,
                               QPointingDevice::PointerType::Pen,
                               QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);

        // Trazo lento y parejo: 2,5 px por muestra, una muestra cada 7,8 ms (~320 px/s).
        // timestampOf(i) da el timestamp (ms) de la muestra i.
        auto medianThickness = [&](auto timestampOf) {
            paintcore::CanvasWidget canvas;
            canvas.resize(300, 100);
            canvas.show();
            if (!QTest::qWaitForWindowExposed(&canvas))
                return -1;
            canvas.setBrush(presetFromJson(segunVelocidad));
            auto send = [&](QEvent::Type type, QPointF pos, qreal pressure, quint64 ts) {
                QTabletEvent event(type, &stylus, pos, canvas.mapToGlobal(pos), pressure, 0, 0, 0, 0, 0,
                                   Qt::NoModifier, Qt::LeftButton,
                                   type == QEvent::TabletRelease ? Qt::NoButton : Qt::LeftButton);
                event.setTimestamp(ts);
                QCoreApplication::sendEvent(&canvas, &event);
            };
            send(QEvent::TabletPress, {20, 50}, 0.5, timestampOf(0));
            for (int i = 1; i <= 100; ++i)
                send(QEvent::TabletMove, {20 + i * 2.5, 50}, 0.5, timestampOf(i));
            send(QEvent::TabletRelease, {270, 50}, 0.0, timestampOf(101));

            const QImage image = canvas.grab().toImage();
            QList<int> thickness;
            for (int x = 40; x <= 250; ++x)
                thickness.append(inkThickness(image, x));
            std::sort(thickness.begin(), thickness.end());
            return thickness[thickness.size() / 2];
        };

        const int parejos = medianThickness([](int i) { return quint64(1000 + i * 7.8); });
        // Como Windows Ink: el timestamp avanza 15,6 ms cada dos muestras.
        const int agrupados = medianThickness([](int i) { return quint64(1000 + (i / 2) * 15.6); });
        QVERIFY2(parejos > 0 && qAbs(agrupados - parejos) <= 1,
                 qPrintable(QStringLiteral("parejos %1 px, agrupados %2 px").arg(parejos).arg(agrupados)));
    }

};

QTEST_MAIN(TestCanvasWidget)
#include "tst_canvaswidget.moc"
