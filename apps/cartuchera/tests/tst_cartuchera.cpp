#include <LeadController.h>
#include <Media.h>
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
        QVERIFY(qAbs(m.sheetX - (2560 - m.sheetWidth) / 2) <= 1); // centrada (redondeo de un píxel)
        // Esquinas de la hoja → celdas 0 y el total.
        QVERIFY(qAbs(m.cellX(m.sheetX)) < 1e-9);
        QVERIFY(qAbs(m.cellY(m.sheetY + m.sheetHeight) - 4795) < 1e-6);
    }

    // HU-52: área calibrada del ultrawide (0, 0, 1734 × 1081) = superficie activa de la
    // Intuos4 L (325,1 × 203,2 mm); la A4 recortada (297 × 203 mm) centrada encima.
    void hojaSobreLaTableta()
    {
        const SheetMapping m = SheetMapping::onTablet(2560, 1080, 0, 0, 1734, 1081, 325.1, 203.2, 297, 203, 7016, 4795);
        const double pxPerMmX = 1734 / 325.1, pxPerMmY = 1081 / 203.2;
        // Esquina de la hoja: 14,05 mm desde el borde izquierdo de la tableta, 0,1 mm desde arriba.
        QVERIFY(qAbs(m.originX - 14.05 * pxPerMmX) < 1e-6);
        QVERIFY(qAbs(m.originY - 0.1 * pxPerMmY) < 1e-6);
        // El centro de la tableta es el centro de la hoja.
        QVERIFY(qAbs(m.cellX(1734 / 2.0) - 7016 / 2.0) < 1.0);
        QVERIFY(qAbs(m.cellY(1081 / 2.0) - 4795 / 2.0) < 1.0);
        // Toda la hoja queda dentro del área (al alcance del lápiz).
        QVERIFY(m.sheetX >= 0 && m.sheetX + m.sheetWidth <= 1734);
        QVERIFY(m.sheetY >= 0 && m.sheetY + m.sheetHeight <= 1081);
    }

    // 100 mm en la tableta son 100 mm en la hoja, en los dos ejes (escala propia por eje).
    void escalaRealEnLosDosEjes()
    {
        const SheetMapping m = SheetMapping::onTablet(2560, 1080, 0, 0, 1734, 1081, 325.1, 203.2, 297, 203, 7016, 4795);
        const double cellsPerMm = 600.0 / 25.4;
        const double dx = m.cellX(100 + 100 * 1734 / 325.1) - m.cellX(100);
        const double dy = m.cellY(100 + 100 * 1081 / 203.2) - m.cellY(100);
        QVERIFY(qAbs(dx / cellsPerMm - 100) < 1e-6);
        QVERIFY(qAbs(dy / cellsPerMm - 100) < 1e-6);
        // Área desplazada (tableta mapeada a otra parte de la pantalla): se corre todo igual.
        const SheetMapping corrida = SheetMapping::onTablet(2560, 1080, 400, 0, 1734, 1081, 325.1, 203.2, 297, 203, 7016, 4795);
        QVERIFY(qAbs(corrida.cellX(500) - m.cellX(100)) < 1e-9);
    }

    void celdasDePixelYVuelta()
    {
        const SheetMapping m = SheetMapping::fit(1580, 1080, 7016, 4795);
        int first, last;
        m.cellsOfColumn(m.sheetX, first, last);
        QCOMPARE(first, 0);
        QVERIFY(last >= 4 && last <= 5); // ~4,4 celdas por píxel
        m.cellsOfColumn(m.sheetX + m.sheetWidth - 1, first, last);
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
        m.cellsOfColumn(m.sheetX + 10, cx0, cx1);
        m.cellsOfRow(m.sheetY + 10, cy0, cy1);
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
    // HU-57: la HB de fábrica es exactamente la calibrada en HU-51 (mismo trazo, mismo
    // hash), y las durezas van de la más clara a la más oscura.
    void durezasDeFabrica()
    {
        const Grades g = factoryGrades();
        QCOMPARE(QLatin1String(kGradeNames[kHbIndex]), QLatin1String("HB"));
        const drymedia::Medium hb = g[kHbIndex].medium(), ref = drymedia::Medium::hb();
        QCOMPARE(hb.softness, ref.softness);
        QCOMPARE(hb.ceiling, ref.ceiling);
        QCOMPARE(hb.forceScale, ref.forceScale);
        QCOMPARE(hb.leadDiameterMm, ref.leadDiameterMm);
        for (int i = 1; i < kGradeCount; ++i) {
            QVERIFY(g[i].softness > g[i - 1].softness);
            QVERIFY(g[i].ceiling >= g[i - 1].ceiling);
        }
    }

    void minaEmpaquetada()
    {
        for (const Lead& l : factoryGrades())
            QCOMPARE(Lead::unpack(l.pack()), l);
        // Fuera de rango se recorta al empaquetar.
        QCOMPARE(Lead::unpack(Lead{999, 5, 70000}.pack()), (Lead{255, 30, 65535}));
    }

    // medios.json: ida y vuelta exacta; lo que falta queda de fábrica, lo desconocido se
    // ignora y lo fuera de rango se recorta; un archivo roto no toca nada.
    void mediosJson()
    {
        MediaSet m;
        m.grades[0] = {9, 66, 21000};
        m.grades[9].diameter = 123;
        m.eraser = {33, 650};
        MediaSet leido;
        leido.eraser = {};
        QVERIFY(mediaFromJson(mediaToJson(m), leido));
        QCOMPARE(leido, m);

        MediaSet parcial;
        QVERIFY(mediaFromJson(R"({"minas": [{"nombre": "4B", "blandura": 300, "techo": 10},
                                            {"nombre": "9H", "blandura": 3}],
                                  "goma": {"diametroMm": 30}})",
                              parcial));
        MediaSet esperado;
        esperado.grades[7].softness = 255;
        esperado.grades[7].ceiling = 2000;
        esperado.eraser.diameter = 800;
        QCOMPARE(parcial, esperado);

        MediaSet intacto = m;
        QString error;
        QVERIFY(!mediaFromJson("{ esto no es json", intacto, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(intacto, m);
    }

    // La goma de fábrica (HU-58) y su paso entre hilos.
    void gomaEmpaquetada()
    {
        const Eraser e;
        QCOMPARE(Eraser::unpack(e.pack()), e);
        QCOMPARE(Eraser::unpack(Eraser{0, 5000}.pack()), (Eraser{1, 800}));
        const drymedia::Medium m = e.medium();
        QVERIFY(m.kind == drymedia::Medium::Kind::Eraser);
        QCOMPARE(m.leadDiameterMm, 5.0);
    }
};

QTEST_GUILESS_MAIN(TestCartuchera)
#include "tst_cartuchera.moc"
