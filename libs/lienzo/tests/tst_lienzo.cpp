#include <lienzo/LeadController.h>
#include <lienzo/Cursor.h>
#include <lienzo/LeadPicker.h>
#include <lienzo/Media.h>
#include <lienzo/SampleQueue.h>
#include <lienzo/SheetMapping.h>
#include <lienzo/Tone.h>
#include <lienzo/ToolPage.h>
#include <lienzo/ViewRotation.h>

#include <drymedia/Paper.h>

#include <appkit/ParamForm.h>

#include <QSlider>
#include <QTest>

#include <thread>

using namespace lienzo;

class TestLienzo : public QObject {
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
    // HU-64: la capa de guías va bajo el grafito. Sin guía, la hoja; con guía y sin
    // grafito, la guía sobre la hoja; con grafito saturado, el grafito la tapa.
    void guiasBajoElGrafito()
    {
        QCOMPARE(paperWithGuide(0), kPaperColor);
        const uint32_t red = 0xFFFF0000u; // rojo opaco, premultiplicado
        QCOMPARE(paperWithGuide(red), red);
        const uint32_t half = 0x80800000u; // rojo al 50 %, premultiplicado
        const uint32_t mixed = paperWithGuide(half);
        QVERIFY((mixed >> 16 & 0xFF) > (kPaperColor >> 16 & 0xFF) - 1);
        QVERIFY((mixed & 0xFF) < (kPaperColor & 0xFF));
        QCOMPARE(toneOf(65535, red), kGraphiteColor);
        QCOMPARE(toneOf(0, red), red);

        drymedia::Paper paper({.seed = 3, .widthMm = 20, .heightMm = 20});
        const SheetMapping m = SheetMapping::fit(100, 100, paper.width(), paper.height());
        std::vector<uint32_t> image(100 * 100), guides(100 * 100, 0);
        guides[50 * 100 + 50] = red;
        renderTone(paper, m, 0, 0, 100, 100, image.data(), guides.data());
        QCOMPARE(image[50 * 100 + 50], red);
        QCOMPARE(image[50 * 100 + 40], kPaperColor);
        // Grafito saturado en las celdas de ese píxel: lo tapa.
        int cx0, cx1, cy0, cy1;
        m.cellsOfColumn(50, cx0, cx1);
        m.cellsOfRow(50, cy0, cy1);
        for (int cy = cy0; cy < cy1; ++cy)
            for (int cx = cx0; cx < cx1; ++cx)
                paper.depositTile(cx / drymedia::kTileSize, cy / drymedia::kTileSize)
                    [size_t(cy % drymedia::kTileSize) * drymedia::kTileSize + size_t(cx % drymedia::kTileSize)] = 65535;
        renderTone(paper, m, 0, 0, 100, 100, image.data(), guides.data());
        QCOMPARE(image[50 * 100 + 50], kGraphiteColor);
        // Hoja nueva: vuelve la guía.
        paper.clear();
        renderTone(paper, m, 0, 0, 100, 100, image.data(), guides.data());
        QCOMPARE(image[50 * 100 + 50], red);
    }

    // HU-67: la muestra de cada mina tiene grafito, y la 6B es más oscura que la 2H.
    void muestrasDeMinas()
    {
        const Grades g = factoryGrades();
        const auto darkness = [](const QImage& image) {
            double sum = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    sum += 255 - qGray(image.pixel(x, y));
            return sum;
        };
        const QImage dura = leadSample(g[0], QSize(300, 56), 0.2258);
        const QImage blanda = leadSample(g[9], QSize(300, 56), 0.2258);
        QCOMPARE(dura.size(), QSize(300, 56));
        const double papel = darkness(leadSample(Lead{1, 87, 2000}, QSize(300, 56), 0.2258));
        const double grafitoDura = darkness(dura) - papel, grafitoBlanda = darkness(blanda) - papel;
        qInfo() << "grafito en la muestra: 2H" << grafitoDura << "· 6B" << grafitoBlanda;
        QVERIFY(grafitoDura > 0);
        QVERIFY(grafitoBlanda > grafitoDura * 1.5);
    }

    // HU-69: con la hoja del tamaño de la tableta, la hoja cubre toda el área útil calibrada.
    void hojaDelTamanoDeLaTableta()
    {
        const drymedia::Paper paper({.widthMm = 325.1, .heightMm = 203.2});
        const SheetMapping m = SheetMapping::onTablet(2560, 1080, 0, 0, 1734, 1081, 325.1, 203.2, 325.1, 203.2,
                                                      paper.width(), paper.height());
        QVERIFY(qAbs(m.sheetX) <= 1);
        QVERIFY(qAbs(m.sheetY) <= 1);
        QVERIFY(qAbs(m.sheetWidth - 1734) <= 1);
        QVERIFY(qAbs(m.sheetHeight - 1081) <= 1);
        QVERIFY(qAbs(m.cellX(1734) - paper.width()) < 2.0);
        QVERIFY(qAbs(m.cellY(1081) - paper.height()) < 2.0);
    }

    // Selector: flechas y Enter eligen; Esc cierra sin elegir.
    void selectorConTeclado()
    {
        LeadPicker picker(nullptr);
        picker.setLeads(factoryGrades(), kHbIndex, 0.2258);
        int elegida = -1, cerrado = 0;
        picker.onPick = [&](int i) { elegida = i; };
        picker.onClose = [&] { ++cerrado; };
        QTest::keyClick(&picker, Qt::Key_Down);
        QTest::keyClick(&picker, Qt::Key_Down);
        QTest::keyClick(&picker, Qt::Key_Return);
        QCOMPARE(elegida, kHbIndex + 2); // 2B
        QTest::keyClick(&picker, Qt::Key_Up);
        QTest::keyClick(&picker, Qt::Key_Escape);
        QCOMPARE(cerrado, 1);
        QCOMPARE(elegida, kHbIndex + 2);
    }

    // HU-12: los controles de la pestaña Lápiz llegan a la mina y a la goma en sus unidades
    // (centésimas de mm, techo de 0 a 65535), y mover uno no toca los demás.
    void controlesDelLapiz()
    {
        ToolPage page;
        const Lead f = factoryGrades()[2]; // F: techo 40632, que en % redondea
        page.setTools(QStringLiteral("F"), f, false, Eraser{}, false);
        Lead recibida;
        Eraser goma;
        page.onLeadChanged = [&](const Lead& l) { recibida = l; };
        page.onEraserChanged = [&](const Eraser& e) { goma = e; };

        QSlider* blandura = page.leadForm()->slider(QStringLiteral("blandura"));
        QVERIFY(blandura);
        blandura->setValue(blandura->value() + 5);
        QCOMPARE(recibida.softness, f.softness + 5);
        QCOMPARE(recibida.ceiling, f.ceiling); // intacto, aunque en % no sea exacto
        QCOMPARE(recibida.diameter, f.diameter);

        QSlider* diametro = page.leadForm()->slider(QStringLiteral("diametro"));
        diametro->setValue(diametro->maximum());
        QCOMPARE(recibida.diameter, 200); // 2,00 mm
        QCOMPARE(recibida.softness, f.softness + 5);

        QSlider* fuerza = page.eraserForm()->slider(QStringLiteral("fuerza"));
        fuerza->setValue(fuerza->minimum());
        QCOMPARE(goma.strength, 1);
        QCOMPARE(goma.diameter, Eraser{}.diameter);

        // Ida y vuelta entre unidades.
        QCOMPARE(leadFrom(leadValues(f), f), f);
        QCOMPARE(leadFrom({{QStringLiteral("techo"), 50}}, f).ceiling, 32768);
    }

    // El selector lleva arriba la imagen de la colección (recurso del lienzo); tocarla no
    // elige ninguna mina.
    void selectorConEncabezado()
    {
        LeadPicker picker(nullptr);
        picker.setLeads(factoryGrades(), kHbIndex, 0.2258);
        const int sinImagen = 44 + kGradeCount * LeadPicker::kRowHeight + 12;
        QVERIFY2(picker.height() > sinImagen + 100, "no se cargó :/lienzo/grafito.jpg");
        int elegida = -1;
        picker.onPick = [&](int i) { elegida = i; };
        QTest::mouseClick(&picker, Qt::LeftButton, {}, QPoint(LeadPicker::kWidth / 2, 40));
        QCOMPARE(elegida, -1);
        // La primera fila queda debajo de la imagen y del título.
        const int primeraFila = picker.height() - 12 - LeadPicker::kRowHeight * kGradeCount;
        QTest::mouseClick(&picker, Qt::LeftButton, {}, QPoint(LeadPicker::kWidth / 2, primeraFila + 10));
        QCOMPARE(elegida, 0);
    }

    // HU-40: la rotación de la vista. Pantalla → imagen deshace imagen → pantalla, el centro
    // no se mueve y 90° lleva la derecha del centro hacia abajo (y hacia abajo: horario).
    void rotacionDeLaVista()
    {
        const ViewRotation r{90, 500, 400};
        double x = 600, y = 400;
        r.toScreen(x, y);
        QVERIFY(qAbs(x - 500) < 1e-9 && qAbs(y - 500) < 1e-9);
        r.toImage(x, y);
        QVERIFY(qAbs(x - 600) < 1e-9 && qAbs(y - 400) < 1e-9);
        double cx = 500, cy = 400;
        ViewRotation{37, 500, 400}.toImage(cx, cy);
        QVERIFY(qAbs(cx - 500) < 1e-9 && qAbs(cy - 400) < 1e-9);
        QCOMPARE(ViewRotation::normalized(190), -170.0);
        QCOMPARE(ViewRotation::normalized(-180), 180.0);
        QCOMPARE(ViewRotation::snapped(22), 15.0);
        QCOMPARE(ViewRotation::snapped(23), 30.0);
    }

    // HU-38: la cruz abierta. Un punto de tinta en el centro y el resto libre; los cuatro
    // brazos tienen tinta y están rodeados de un borde claro.
    void punteroCruzAbierta()
    {
        const auto px = crossCursorPixels();
        const auto at = [&](int x, int y) { return px[size_t(y) * kCursorSize + size_t(x)]; };
        const int c = kCursorHotspot;
        // Punto central de tinta con su borde claro; a 2 px, libre.
        QCOMPARE(at(c, c), 0xFF111111u);
        for (const auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}})
            QCOMPARE(at(c + dx, c + dy), 0xFFF5F0E6u);
        for (const auto [dx, dy] : {std::pair{2, 0}, {-2, 0}, {0, 2}, {0, -2}, {2, 2}, {-2, -2}})
            QCOMPARE(at(c + dx, c + dy), 0u);
        for (const auto [x, y] : {std::pair{c + 6, c}, {c - 6, c}, {c, c + 6}, {c, c - 6}}) {
            QCOMPARE(at(x, y), 0xFF111111u);
            QVERIFY(at(x, y - 1) == 0xFFF5F0E6u || at(x - 1, y) == 0xFFF5F0E6u);
        }
        QCOMPARE(at(0, 0), 0u);
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

QTEST_MAIN(TestLienzo)
#include "tst_lienzo.moc"
