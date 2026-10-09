#include <LeadController.h>
#include <SampleQueue.h>
#include <SheetMapping.h>
#include <Tone.h>

#include <drymedia/Paper.h>

#include <QTest>

#include <thread>

using namespace cartuchera;

class TestCartuchera : public QObject {
    Q_OBJECT

private slots:
    // La A4 (7016 × 4795 celdas) en el ultrawide (2560 × 1080): la limita la altura.
    void hojaAjustadaALaVentana()
    {
        const SheetMapping m = SheetMapping::fit(2560, 1080, 7016, 4795);
        QCOMPARE(m.sheetHeight, 1080);
        QVERIFY(m.sheetWidth <= 2560);
        QVERIFY(qAbs(double(m.sheetWidth) / m.sheetHeight - 7016.0 / 4795.0) < 0.01);
        QCOMPARE(m.sheetX, (2560 - m.sheetWidth) / 2);
        // Esquinas de la hoja → celdas 0 y el total.
        QVERIFY(qAbs(m.cellX(m.sheetX)) < 1e-9);
        QVERIFY(qAbs(m.cellY(m.sheetY + m.sheetHeight) - 4795) < 1e-6);
    }

    void celdasDePixelYVuelta()
    {
        const SheetMapping m = SheetMapping::fit(1580, 1080, 7016, 4795);
        int first, last;
        m.cellsOfPixel(0, m.cellsWidth, first, last);
        QCOMPARE(first, 0);
        QVERIFY(last >= 4 && last <= 5); // ~4,4 celdas por píxel
        m.cellsOfPixel(m.sheetWidth - 1, m.cellsWidth, first, last);
        QCOMPARE(last, m.cellsWidth);

        int px0, py0, px1, py1;
        QVERIFY(m.pixelsOfCells(100, 100, 120, 110, px0, py0, px1, py1));
        QVERIFY(px1 > px0 && py1 > py0);
        QVERIFY(qAbs(m.cellX(px0) - 100) < 5 && qAbs(m.cellX(px1) - 120) < 5);
        QVERIFY(!m.pixelsOfCells(-50, -50, -10, -10, px0, py0, px1, py1)); // fuera de la hoja
    }

    void tonoDeHojaAGrafito()
    {
        QCOMPARE(toneOf(0), kPaperColor);
        QCOMPARE(toneOf(65535), kGraphiteColor);
        const uint32_t medio = toneOf(32768);
        const auto rojo = [](uint32_t c) { return int((c >> 16) & 0xFF); };
        QVERIFY(rojo(medio) < rojo(kPaperColor) && rojo(medio) > rojo(kGraphiteColor));
    }

    // Cada píxel promedia las celdas que cubre: una sola celda saturada oscurece apenas
    // su píxel, y nada fuera de él.
    void renderPromediaLasCeldas()
    {
        drymedia::Paper paper({.seed = 1, .widthMm = 40, .heightMm = 30});
        const SheetMapping m = SheetMapping::fit(200, 150, paper.width(), paper.height());
        std::vector<uint32_t> image(size_t(200) * 150, 0);
        renderTone(paper, m, 0, 0, 200, 150, image.data());
        QCOMPARE(image[size_t(m.sheetY + 10) * 200 + size_t(m.sheetX + 10)], kPaperColor);
        if (m.sheetX > 0)
            QCOMPARE(image[size_t(m.sheetY + 10) * 200], kOutsideColor);

        // Saturar todas las celdas del píxel (sheetX + 10, sheetY + 10).
        int cx0, cx1, cy0, cy1;
        m.cellsOfPixel(10, m.cellsWidth, cx0, cx1);
        m.cellsOfPixel(10, m.cellsHeight, cy0, cy1);
        for (int y = cy0; y < cy1; ++y)
            for (int x = cx0; x < cx1; ++x)
                paper.depositTile(x / drymedia::kTileSize, y / drymedia::kTileSize)[(y % drymedia::kTileSize) * drymedia::kTileSize + x % drymedia::kTileSize] = 65535;
        renderTone(paper, m, 0, 0, 200, 150, image.data());
        QCOMPARE(image[size_t(m.sheetY + 10) * 200 + size_t(m.sheetX + 10)], kGraphiteColor);
        QCOMPARE(image[size_t(m.sheetY + 10) * 200 + size_t(m.sheetX + 11)], kPaperColor);
        QCOMPARE(image[size_t(m.sheetY + 11) * 200 + size_t(m.sheetX + 10)], kPaperColor);
    }

    void colaEntreHilos()
    {
        SampleQueue queue;
        std::thread producer([&] {
            for (int i = 0; i < 100; ++i) {
                tabletinput::PenSample s;
                s.timeUs = i;
                queue.push({s});
            }
        });
        std::vector<tabletinput::PenSample> all, batch;
        while (all.size() < 100) {
            queue.wait(100);
            queue.takeAll(batch);
            all.insert(all.end(), batch.begin(), batch.end());
        }
        producer.join();
        for (int i = 0; i < 100; ++i)
            QCOMPARE(all[size_t(i)].timeUs, int64_t(i)); // todas, en orden
    }

    void adelantoAdaptativo()
    {
        LeadController lead;
        QCOMPARE(lead.leadMs(), LeadController::kStartMs);
        lead.onFrame(true);
        QCOMPARE(lead.leadMs(), LeadController::kStartMs + LeadController::kMissStepMs);
        for (int i = 0; i < 20; ++i)
            lead.onFrame(true);
        QCOMPARE(lead.leadMs(), LeadController::kMaxMs); // techo
        // Muchos frames tranquilos: baja de a poco hasta el piso.
        for (int i = 0; i < LeadController::kCalmFrames * 100; ++i)
            lead.onFrame(false);
        QCOMPARE(lead.leadMs(), LeadController::kMinMs);
        QCOMPARE(lead.missed(), 21);
        // Un frame perdido corta la racha: hacen falta kCalmFrames seguidos para bajar.
        lead.onFrame(true);
        const double after = lead.leadMs();
        for (int i = 0; i < LeadController::kCalmFrames - 1; ++i)
            lead.onFrame(false);
        QCOMPARE(lead.leadMs(), after);
    }
};

QTEST_GUILESS_MAIN(TestCartuchera)
#include "tst_cartuchera.moc"
