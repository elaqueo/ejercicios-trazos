#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QImage>
#include <QPainter>
#include <QTest>

namespace {

QImage sheet()
{
    QImage image(120, 120, QImage::Format_RGB32);
    image.fill(appkit::theme::kHoja);
    return image;
}

} // namespace

class TestGuides : public QObject {
    Q_OBJECT

private slots:
    // Punto a unir: centro y anillo en azul tinta, hoja entre los dos.
    void puntoAUnir()
    {
        QImage image = sheet();
        {
            QPainter p(&image);
            p.setRenderHint(QPainter::Antialiasing);
            appkit::guides::targetPoint(p, {60, 60});
        }
        QCOMPARE(image.pixelColor(60, 60), appkit::theme::kGuia);
        QCOMPARE(image.pixelColor(60, 60 - appkit::theme::kRadioPuntoUnir), appkit::theme::kGuia);
        QCOMPARE(image.pixelColor(60, 52), appkit::theme::kHoja);
        QCOMPARE(image.pixelColor(60, 90), appkit::theme::kHoja);
    }

    // Punto de paso: anillo hueco (el centro queda en la hoja).
    void puntoDePaso()
    {
        QImage image = sheet();
        {
            QPainter p(&image);
            appkit::guides::passPoint(p, {60, 60});
        }
        QCOMPARE(image.pixelColor(60, 60), appkit::theme::kHoja);
        QCOMPARE(image.pixelColor(60, 60 - appkit::theme::kRadioPuntoPaso), appkit::theme::kGuia);
    }

    void lineaGuiaYConstruccion()
    {
        QImage image = sheet();
        {
            QPainter p(&image);
            appkit::guides::guideLine(p, {10, 30}, {110, 30});
            appkit::guides::constructionLine(p, {10, 80}, {110, 80});
        }
        QCOMPARE(image.pixelColor(60, 30), appkit::theme::kGuia);
        // Punteada: a lo largo de la línea hay tramos de color y huecos de hoja.
        int conColor = 0, huecos = 0;
        for (int x = 12; x < 108; ++x)
            (image.pixelColor(x, 80) == appkit::theme::kHoja ? huecos : conColor) += 1;
        QVERIFY2(conColor > 20 && huecos > 20, qPrintable(QStringLiteral("%1 con color, %2 huecos").arg(conColor).arg(huecos)));
    }

    // Flecha: punto de arranque y cuerpo en ámbar.
    void flechaDeDireccion()
    {
        QImage image = sheet();
        {
            QPainter p(&image);
            p.setRenderHint(QPainter::Antialiasing);
            appkit::guides::directionArrow(p, {20, 100}, {100, 20});
        }
        QCOMPARE(image.pixelColor(20, 100), appkit::theme::kEnfasis);
        QCOMPARE(image.pixelColor(60, 60), appkit::theme::kEnfasis);
    }
};

QTEST_MAIN(TestGuides)
#include "tst_guides.moc"
