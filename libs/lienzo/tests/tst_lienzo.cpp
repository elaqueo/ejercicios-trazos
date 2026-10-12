#include <lienzo/LeadController.h>
#include <lienzo/Compositor.h>
#include <lienzo/AudioStrip.h>
#include <lienzo/Cursor.h>
#include <lienzo/FieldGuide.h>
#include <lienzo/FixedSheet.h>
#include <lienzo/LeadPicker.h>
#include <lienzo/LightTable.h>
#include <lienzo/Media.h>
#include <lienzo/Renderer.h>
#include <lienzo/DisplayImage.h>
#include <lienzo/SampleQueue.h>
#include <appkit/Theme.h>
#include <lienzo/SheetImages.h>
#include <lienzo/SheetPanel.h>
#include <lienzo/SheetStack.h>
#include <lienzo/SheetMapping.h>
#include <lienzo/Simulation.h>
#include <lienzo/Take.h>
#include <lienzo/TakeSaver.h>
#include <lienzo/Tone.h>
#include <lienzo/ToolPage.h>
#include <lienzo/ViewRotation.h>

#include <drymedia/Medium.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>
#include <drymedia/Tip.h>

#include <appkit/ParamForm.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <thread>

using namespace lienzo;

namespace {

// Relieve medio por píxel de la hoja, como lo ve la pantalla (HU-75): el detalle del grano.
std::vector<double> meanRelief(const drymedia::Paper& paper, const SheetMapping& m)
{
    std::vector<double> mean(size_t(m.sheetWidth) * size_t(m.sheetHeight));
    for (int y = 0; y < m.sheetHeight; ++y) {
        int cy0, cy1;
        m.cellsOfRow(m.sheetY + y, cy0, cy1);
        for (int x = 0; x < m.sheetWidth; ++x) {
            int cx0, cx1;
            m.cellsOfColumn(m.sheetX + x, cx0, cx1);
            double s = 0;
            for (int cy = cy0; cy < cy1; ++cy)
                for (int cx = cx0; cx < cx1; ++cx)
                    s += paper.reliefAt(cx, cy);
            mean[size_t(y) * size_t(m.sheetWidth) + size_t(x)] = s / ((cx1 - cx0) * (cy1 - cy0));
        }
    }
    // Solo el detalle, como la textura: menos el promedio del cuadrado alrededor.
    const int w = m.sheetWidth, h = m.sheetHeight, r = PaperTexture::kDetailRadius;
    std::vector<double> detail(mean.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double s = 0;
            int count = 0;
            for (int yy = std::max(y - r, 0); yy < std::min(y + r + 1, h); ++yy)
                for (int xx = std::max(x - r, 0); xx < std::min(x + r + 1, w); ++xx, ++count)
                    s += mean[size_t(yy) * size_t(w) + size_t(xx)];
            detail[size_t(y) * size_t(w) + size_t(x)] = mean[size_t(y) * size_t(w) + size_t(x)] - s / count;
        }
    return detail;
}

double correlation(const std::vector<double>& a, const std::vector<double>& b)
{
    double ma = 0, mb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        ma += a[i];
        mb += b[i];
    }
    ma /= double(a.size());
    mb /= double(b.size());
    double ab = 0, aa = 0, bb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        ab += (a[i] - ma) * (b[i] - mb);
        aa += (a[i] - ma) * (a[i] - ma);
        bb += (b[i] - mb) * (b[i] - mb);
    }
    return ab / std::sqrt(aa * bb);
}

} // namespace

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

    // HU-97: el color de la hoja es un dato del lienzo: llega a la imagen y la mesa de luz
    // tiñe contra él (una hoja de abajo sin trazo, del color del papel, no tiñe nada).
    void colorDelPapel()
    {
        constexpr uint32_t animation = bgra(0xFA, 0xF8, 0xF2), gray = bgra(0xE0, 0xE0, 0xE0);
        drymedia::Paper paper({.seed = 1, .widthMm = 40, .heightMm = 30});
        const SheetMapping m = SheetMapping::fit(200, 150, paper.width(), paper.height());
        std::vector<uint32_t> image(size_t(200) * 150, 0);
        renderTone(paper, m, 0, 0, 200, 150, image.data(), nullptr, nullptr, animation);
        QCOMPARE(image[size_t(m.sheetY + 10) * 200 + size_t(m.sheetX + 10)], animation);
        QCOMPARE(paperWithGuide(0, animation), animation);

        const LightTableLayer red{nullptr, bgra(0xC8, 0x35, 0x2E), 1.0f};
        QCOMPARE(Compositor::reference(gray, &gray, &red, 1, gray), gray); // del color del papel: sin tinte
        QVERIFY(Compositor::reference(gray, &gray, &red, 1) != gray);      // contra el de Cartuchera, gris es trazo
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

    // HU-100: la muestra de cada lápiz del selector sale de su color; la del grafito, gris.
    // Sobre papel blanco: en un trazo de color, su canal es el que menos baja.
    void muestrasDeLosLapices()
    {
        const double cellsPerMm = 300 / 25.4, pixelsPerCell = 0.45;
        const uint32_t white = bgra(255, 255, 255);
        const auto tinted = [](const QImage& image, int channel) { // píxeles con trazo de ese color
            int n = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const QColor c = image.pixelColor(x, y);
                    const int d[3] = {255 - c.red(), 255 - c.green(), 255 - c.blue()};
                    const int others = std::min(d[(channel + 1) % 3], d[(channel + 2) % 3]);
                    n += d[0] + d[1] + d[2] > 30 && d[channel] < others - 15;
                }
            return n;
        };
        const QSize size(150, 52);
        const auto sample = [&](const drymedia::Medium& m) { return mediumSample(m, size, pixelsPerCell, cellsPerMm, white, true); };
        const QImage grafito = sample(forProfile(Lead{}.medium(), MediumProfile::Animation));
        const QImage azul = sample(drymedia::Medium::colored(0)), rojo = sample(drymedia::Medium::colored(1)),
                     verde = sample(drymedia::Medium::colored(2));
        qInfo() << "píxeles teñidos: azul" << tinted(azul, 2) << "· rojo" << tinted(rojo, 0) << "· verde" << tinted(verde, 1);
        QCOMPARE(tinted(grafito, 0) + tinted(grafito, 1) + tinted(grafito, 2), 0);
        QVERIFY(tinted(azul, 2) > 50);
        QVERIFY(tinted(rojo, 0) > 50);
        QVERIFY(tinted(verde, 1) > 50);
        QCOMPARE(tinted(azul, 0), 0);
    }

    // HU-99: los lápices de color van sobre la hoja y bajo el grafito.
    void colorBajoElGrafito()
    {
        drymedia::Paper paper({.seed = 1, .widthMm = 40, .heightMm = 30});
        const SheetMapping m = SheetMapping::fit(200, 150, paper.width(), paper.height());
        std::vector<uint32_t> image(size_t(200) * 150, 0);
        const auto fill = [&](int px, int py, int plane, uint16_t value) {
            int cx0, cx1, cy0, cy1;
            m.cellsOfColumn(m.sheetX + px, cx0, cx1);
            m.cellsOfRow(m.sheetY + py, cy0, cy1);
            for (int y = cy0; y < cy1; ++y)
                for (int x = cx0; x < cx1; ++x) {
                    const int tx = x / drymedia::kTileSize, ty = y / drymedia::kTileSize;
                    paper.depositTile(tx, ty)[plane * drymedia::kTileCells + (y % drymedia::kTileSize) * drymedia::kTileSize +
                                              x % drymedia::kTileSize] = value;
                    if (plane)
                        paper.markTilePlanes(tx, ty, uint8_t(1 << plane));
                }
        };
        const uint16_t azul = drymedia::colorCell(0, 65535);
        fill(10, 10, drymedia::kColorPlane, azul); // solo azul
        fill(20, 10, drymedia::kColorPlane, azul); // azul con grafito saturado encima
        fill(20, 10, drymedia::kDepositPlane, 65535);
        renderTone(paper, m, 0, 0, 200, 150, image.data());
        QCOMPARE(image[size_t(m.sheetY + 10) * 200 + size_t(m.sheetX + 10)],
                 mixTone(kPaperColor, kColorPencilColors[0], drymedia::cellColorAmount(azul)));
        QCOMPARE(image[size_t(m.sheetY + 10) * 200 + size_t(m.sheetX + 20)], kGraphiteColor);
        QCOMPARE(image[size_t(m.sheetY + 10) * 200 + size_t(m.sheetX + 15)], kPaperColor);
    }

    // Textura del papel (HU-75), con 0 % la imagen de siempre: sin factor, y un factor
    // neutro (4096) da exactamente lo mismo.
    void texturaEnCeroEsLaImagenDeHoy()
    {
        drymedia::Paper paper({.seed = 1, .widthMm = 40, .heightMm = 30});
        const SheetMapping m = SheetMapping::fit(200, 150, paper.width(), paper.height());
        PaperTexture texture;
        texture.build(paper, m);
        texture.setLight(225, 0);
        QVERIFY(texture.shade() == nullptr);

        std::vector<uint32_t> plain(size_t(200) * 150), neutral(plain.size());
        const std::vector<uint16_t> one(plain.size(), 4096);
        paper.depositTile(1, 1)[100] = 30000;
        renderTone(paper, m, 0, 0, 200, 150, plain.data());
        renderTone(paper, m, 0, 0, 200, 150, neutral.data(), nullptr, one.data());
        QCOMPARE(neutral, plain);
    }

    // El brillo sigue al relieve del lápiz: la cara que mira a la luz se aclara (al dar
    // vuelta la luz se invierte) y, con luz de los dos lados, las crestas quedan más claras.
    void brilloSigueAlRelieve()
    {
        drymedia::Paper paper({.seed = 3, .widthMm = 40, .heightMm = 30});
        const SheetMapping m = SheetMapping::fit(200, 150, paper.width(), paper.height());
        const std::vector<double> mean = meanRelief(paper, m);
        PaperTexture texture;
        texture.build(paper, m);
        const auto factors = [&](double degrees) {
            texture.setLight(degrees, 1.0);
            std::vector<double> f(mean.size());
            for (int y = 0; y < m.sheetHeight; ++y)
                for (int x = 0; x < m.sheetWidth; ++x)
                    f[size_t(y) * size_t(m.sheetWidth) + size_t(x)] =
                        texture.shade()[size_t(m.sheetY + y) * 200 + size_t(m.sheetX + x)];
            return f;
        };
        const std::vector<double> right = factors(0), left = factors(180);

        // Luz a la derecha: se aclara donde el relieve baja hacia la derecha.
        std::vector<double> difference, fallsRight, sumLights, inner;
        for (int y = 1; y + 1 < m.sheetHeight; ++y)
            for (int x = 1; x + 1 < m.sheetWidth; ++x) {
                const size_t i = size_t(y) * size_t(m.sheetWidth) + size_t(x);
                difference.push_back(right[i] - left[i]);
                fallsRight.push_back(mean[i - 1] - mean[i + 1]);
                sumLights.push_back(right[i] + left[i]);
                inner.push_back(mean[i]);
            }
        const double slope = correlation(difference, fallsRight), height = correlation(sumLights, inner);
        qInfo() << "correlación pendiente" << slope << "altura" << height;
        QVERIFY(slope > 0.9);
        QVERIFY(height > 0.9);
        // Y se nota: al 100 % el brillo varía más de un 10 %.
        const auto [lo, hi] = std::minmax_element(right.begin(), right.end());
        QVERIFY(*hi - *lo > 0.1 * 4096);
    }

    // El grafito tapa la textura: saturado, el píxel es grafito con cualquier luz; a medio
    // llenar, la textura se ve menos que en la hoja limpia.
    void grafitoSaturadoApagaLaTextura()
    {
        drymedia::Paper paper({.seed = 1, .widthMm = 40, .heightMm = 30});
        const SheetMapping m = SheetMapping::fit(200, 150, paper.width(), paper.height());
        PaperTexture texture;
        texture.build(paper, m);
        texture.setLight(225, 1.0);
        const auto spread = [&](uint16_t deposit) {
            for (int ty = 0; ty < paper.tilesY(); ++ty)
                for (int tx = 0; tx < paper.tilesX(); ++tx)
                    std::fill_n(paper.depositTile(tx, ty), drymedia::kTileCells, deposit);
            std::vector<uint32_t> image(size_t(200) * 150);
            renderTone(paper, m, 0, 0, 200, 150, image.data(), nullptr, texture.shade());
            int lo = 255, hi = 0;
            for (int y = m.sheetY; y < m.sheetY + m.sheetHeight; ++y)
                for (int x = m.sheetX; x < m.sheetX + m.sheetWidth; ++x) {
                    const int g = int((image[size_t(y) * 200 + size_t(x)] >> 8) & 0xFF);
                    lo = std::min(lo, g);
                    hi = std::max(hi, g);
                }
            return hi - lo;
        };
        const int clean = spread(0), half = spread(32768), full = spread(65535);
        qInfo() << "rango del verde: limpia" << clean << "media" << half << "saturada" << full;
        QVERIFY(clean >= 10);
        QVERIFY(half < clean);
        QCOMPARE(full, 0);
    }

    // La luz de la textura sigue al giro de la vista cuando el giro se queda quieto (la rueda
    // de la tableta, HU-74, gira muchas veces por segundo); la intensidad, en el acto.
    void luzEsperaQueElGiroSeQuedeQuieto()
    {
        LightSchedule light;
        QVERIFY(light.due(0, 20, 0));       // la primera vez, siempre
        QVERIFY(!light.due(0, 20, 10));     // nada cambió
        QVERIFY(!light.due(5, 20, 20));     // girando…
        QVERIFY(!light.due(10, 20, 100));
        QVERIFY(!light.due(15, 20, 250));   // sigue girando: espera de nuevo
        QVERIFY(!light.due(15, 20, 400));
        QVERIFY(light.due(15, 20, 460));    // quieto 210 ms: ahora sí
        QVERIFY(!light.due(15, 20, 500));
        QVERIFY(light.due(15, 30, 510));    // otra intensidad: en el acto
        QVERIFY(light.due(20, 0, 520));     // apagar: en el acto
        QVERIFY(!light.due(40, 0, 530));    // sin textura el giro no importa
        QVERIFY(!light.due(40, 0, 900));
        QVERIFY(light.due(40, 20, 910));    // al prenderla, con el giro de ahora
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

    // HU-101: con zoom, pantalla → imagen sigue deshaciendo imagen → pantalla (con giro y
    // corrimiento); el zoom hacia un punto lo deja quieto; los límites, de 25 % a 400 %; 100 %
    // centrada es la identidad.
    void zoomDeLaVista()
    {
        const ViewRotation r{30, 500, 400, 2.5, 40, -70};
        double x = 612, y = 333;
        r.toScreen(x, y);
        r.toImage(x, y);
        QVERIFY(qAbs(x - 612) < 1e-9 && qAbs(y - 333) < 1e-9);
        double ix = 600, iy = 400; // a 2x sin giro, 100 px de imagen son 200 de pantalla
        ViewRotation{0, 500, 400, 2.0}.toScreen(ix, iy);
        QVERIFY(qAbs(ix - 700) < 1e-9 && qAbs(iy - 400) < 1e-9);

        const ViewRotation start{15, 500, 400};
        double qx = 820, qy = 610; // la punta
        double before_x = qx, before_y = qy;
        start.toImage(before_x, before_y);
        const ViewRotation zoomed = start.zoomedAt(3.0, qx, qy);
        QCOMPARE(zoomed.scale, 3.0);
        double after_x = qx, after_y = qy;
        zoomed.toImage(after_x, after_y);
        QVERIFY(qAbs(after_x - before_x) < 1e-6 && qAbs(after_y - before_y) < 1e-6); // lo de bajo la punta, quieto

        QCOMPARE(start.zoomedAt(10, qx, qy).scale, ViewRotation::kMaxScale);
        QCOMPARE(start.zoomedAt(0.01, qx, qy).scale, ViewRotation::kMinScale);
        // Acercar y alejar hacia el mismo punto vuelve a la vista de antes.
        QVERIFY((ViewRotation{0, 500, 400}.zoomedAt(2, 700, 100).zoomedAt(1, 700, 100).identity()));
    }

    // HU-102: el corrimiento mueve la hoja lo mismo que la punta, con giro y zoom; el centro de
    // la hoja no sale del cliente.
    void panDeLaVista()
    {
        ViewRotation v{40, 500, 400, 2.0};
        double ix = 650, iy = 300; // un punto de la hoja, bajo la punta
        double sx = ix, sy = iy;
        v.toScreen(sx, sy);
        v.panX += 120; // la punta se movió (120, −80)
        v.panY -= 80;
        double ax = ix, ay = iy;
        v.toScreen(ax, ay);
        QVERIFY(qAbs(ax - (sx + 120)) < 1e-9 && qAbs(ay - (sy - 80)) < 1e-9);

        const ViewRotation lejos = ViewRotation{0, 500, 400, 1, 3000, -900}.clampedPan(2560, 1080);
        QCOMPARE(lejos.panX, 2560.0 - 500); // el centro, contra el borde derecho
        QCOMPARE(lejos.panY, -400.0);       // y contra el de arriba
        const ViewRotation cerca = ViewRotation{0, 500, 400, 1, 30, 20}.clampedPan(2560, 1080);
        QCOMPARE(cerca.panX, 30.0);
        QCOMPARE(cerca.panY, 20.0);
    }

    // HU-79: Shift + arrastrar gira lo que recorre la punta, sin saltar de a 15°.
    void arrastreGiraLibre()
    {
        QCOMPARE(ViewRotation::dragged(0, 10, 32), 22.0);
        QCOMPARE(ViewRotation::dragged(30, -170, 175), 15.0); // la punta cruza ±180: −15°
        QCOMPARE(ViewRotation::dragged(7.5, 0, 0.25), 7.75);
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

    // HU-73: con el costado apagado, cualquier inclinación da la punta vertical.
    void costadoApagado()
    {
        const drymedia::Medium m = Lead{}.medium();
        const drymedia::Tip vertical = drymedia::Tip::make(m, 0, 90);
        for (const float alt : {30.0f, 55.0f, 89.0f}) {
            QCOMPARE(penAltitude(alt, true), alt);
            const drymedia::Tip tip = drymedia::Tip::make(m, 137, penAltitude(alt, false));
            QCOMPARE(tip.width(), vertical.width());
            QVERIFY(std::equal(tip.heights(), tip.heights() + tip.cells(), vertical.heights()));
            QVERIFY(drymedia::Tip::make(m, 137, alt).cellsInside() != vertical.cellsInside());
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
        m.grades[4].burnish = 7700;
        m.eraser = {33, 650};
        for (Lead& l : m.grades)
            l.halfAngle = 15, l.minAltitude = 35, l.wear = 9; // la punta (HU-59, HU-62), una sola vez en el archivo
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

    // HU-82: la activa sigue a su hoja al agregar, quitar y mover.
    void pilaOrden()
    {
        SheetOrder o;
        o.add(1); // [0*, n]
        QCOMPARE(o.count, 2);
        QCOMPARE(o.active, 0);
        o.add(0); // [n, 0*, n]
        QCOMPARE(o.active, 1);
        o.move(1, 2); // [n, n, 0*]
        QCOMPARE(o.active, 2);
        o.move(0, 2); // la de antes pasa al final: la activa corre una
        QCOMPARE(o.active, 1);
        QVERIFY(o.remove(1)); // quitar la activa: pasa la que queda en su lugar
        QCOMPARE(o.count, 2);
        QCOMPARE(o.active, 1);
        QVERIFY(o.remove(1)); // la activa era la última: pasa la anterior
        QCOMPARE(o.active, 0);
        QVERIFY(!o.remove(0)); // con una sola hoja no se quita
        o.activate(5);         // fuera de rango: nada
        QCOMPARE(o.active, 0);

        // HU-83: cada hoja conserva su id al moverla, y los ids no se reusan.
        SheetOrder p;
        p.add(1);
        p.add(2); // ids [1, 2, 3]
        p.move(0, 2);
        QCOMPARE(p.ids, (std::vector<uint64_t>{2, 3, 1}));
        QCOMPARE(p.activeId(), uint64_t(1));
        p.remove(0);
        p.add(0);
        QCOMPARE(p.ids, (std::vector<uint64_t>{4, 3, 1}));
    }

    // HU-82: cada hoja guarda su depósito y su deshacer; el papel trabaja con el de la activa.
    void pilaDeHojas()
    {
        drymedia::Paper paper({.seed = 3, .widthMm = 80, .heightMm = 60});
        SheetStack stack(100);
        drymedia::Pencil pencil(paper, drymedia::Medium::hb());
        pencil.setTileObserver([&stack](int i, const uint16_t* before, uint8_t planes) {
            stack.history().beforeTileWrite(i, before, planes);
        });
        const auto stroke = [&](double y) {
            stack.history().beginStroke();
            pencil.beginStroke({200, y, 0.6f, 0, 80});
            for (int i = 1; i <= 30; ++i)
                pencil.strokeTo({200 + 1000.0 * i / 30, y, 0.6f, 0, 80});
            pencil.endStroke();
            stack.history().endStroke(paper);
        };
        const uint64_t blank = paper.reliefHash();

        stroke(300);
        const uint64_t h0 = paper.hash();
        QVERIFY(!stack.add(paper, 1)); // agregar no cambia la hoja del papel
        QCOMPARE(stack.count(), 2);
        QCOMPARE(paper.hash(), h0);
        QVERIFY(stack.activate(paper, 1));
        QCOMPARE(paper.hash(), blank); // hoja nueva, en blanco
        stroke(800);
        const uint64_t h1 = paper.hash();
        QVERIFY(h1 != h0);
        QCOMPARE(stack.hash(paper, 0), h0);
        QCOMPARE(paper.hash(), h1); // hash() devuelve la hoja a su lugar

        // Deshacer por hoja: volver a la 0 y deshacer borra su trazo, no el de la 1.
        stack.activate(paper, 0);
        QCOMPARE(paper.hash(), h0);
        stack.history().undo(paper);
        QCOMPARE(paper.hash(), blank);
        QCOMPARE(stack.hash(paper, 1), h1);
        stack.history().redo(paper);
        QCOMPARE(paper.hash(), h0);

        // Mover conserva el contenido y cuál es la activa.
        QVERIFY(!stack.move(0, 1));
        QCOMPARE(stack.active(), 1);
        QCOMPARE(paper.hash(), h0);
        QCOMPARE(stack.hash(paper, 0), h1);

        // Quitar la activa: entra la que queda.
        QVERIFY(stack.remove(paper, 1));
        QCOMPARE(stack.count(), 1);
        QCOMPARE(stack.active(), 0);
        QCOMPARE(paper.hash(), h1);
        QVERIFY(!stack.remove(paper, 0)); // la única no se quita
    }

    // HU-87: insertar, duplicar, limpiar y borrar se deshacen y rehacen; Z deshace lo más
    // reciente entre el último trazo de la hoja activa y la última operación de la pila.
    void pilaDeshacer()
    {
        drymedia::Paper paper({.seed = 3, .widthMm = 80, .heightMm = 60});
        SheetStack stack(100);
        std::vector<uint64_t> left; // hojas que dejaron de ser la activa (onLeave)
        stack.onLeave = [&left](uint64_t id) { left.push_back(id); };
        drymedia::Pencil pencil(paper, drymedia::Medium::hb());
        pencil.setTileObserver([&stack](int i, const uint16_t* before, uint8_t planes) {
            stack.history().beforeTileWrite(i, before, planes);
        });
        uint64_t tag = 0;
        const auto stroke = [&](double y) {
            stack.history().beginStroke(++tag);
            stack.clearRedo();
            pencil.beginStroke({200, y, 0.6f, 0, 80});
            for (int i = 1; i <= 30; ++i)
                pencil.strokeTo({200 + 1000.0 * i / 30, y, 0.6f, 0, 80});
            pencil.endStroke();
            stack.history().endStroke(paper);
        };
        const uint64_t blank = paper.reliefHash();

        stroke(300); // tag 1, en la hoja 1
        const uint64_t a = paper.hash();
        QVERIFY(stack.duplicateActive(paper, ++tag)); // tag 2: copia de A, después, y activa
        QCOMPARE(stack.count(), 2);
        QCOMPARE(stack.active(), 1);
        QCOMPARE(paper.hash(), a);
        QCOMPARE(left, std::vector<uint64_t>{1});
        stroke(800); // tag 3, sobre la copia
        const uint64_t copy = paper.hash();
        QVERIFY(copy != a);
        QCOMPARE(stack.hash(paper, 0), a); // la original no cambió

        // Z: primero el trazo de la copia, después la copia misma (y vuelve a la original).
        QVERIFY(!stack.undo(paper).sheetChanged);
        QCOMPARE(paper.hash(), a);
        QVERIFY(stack.undo(paper).sheetChanged);
        QCOMPARE(stack.count(), 1);
        QCOMPARE(paper.hash(), a);
        // Ctrl+Y en el orden inverso: la copia vuelve, con su id, y después su trazo.
        QVERIFY(stack.redo(paper).sheetChanged);
        QCOMPARE(stack.count(), 2);
        QCOMPARE(stack.order().ids, (std::vector<uint64_t>{1, 2}));
        QCOMPARE(stack.active(), 1);
        QVERIFY(!stack.redo(paper).sheetChanged);
        QCOMPARE(paper.hash(), copy);

        // Insertar en blanco después de la activa (tag 4); limpiar (5) y borrar (6) se deshacen.
        QVERIFY(stack.insertAfterActive(paper, ++tag));
        QCOMPARE(stack.active(), 2);
        QCOMPARE(paper.hash(), blank);
        stack.activate(paper, 1);
        QVERIFY(stack.clearActive(paper, ++tag));
        QCOMPARE(paper.hash(), blank);
        QVERIFY(!stack.undo(paper).sheetChanged); // limpiar es como un trazo
        QCOMPARE(paper.hash(), copy);
        QVERIFY(stack.remove(paper, 1, ++tag)); // borrar la activa
        QCOMPARE(stack.count(), 2);
        QVERIFY(stack.undo(paper).sheetChanged); // vuelve en su lugar, activa
        QCOMPARE(stack.count(), 3);
        QCOMPARE(stack.active(), 1);
        QCOMPARE(paper.hash(), copy);
        QCOMPARE(stack.order().ids, (std::vector<uint64_t>{1, 2, 3}));
        // Lo más reciente ahora es la inserción de la hoja 3 (tag 4), no el trazo de la copia
        // (tag 3): Z se lleva la hoja 3. Y después sí, el trazo, que volvió con su hoja.
        QVERIFY(stack.undo(paper).sheetChanged);
        QCOMPARE(stack.order().ids, (std::vector<uint64_t>{1, 2}));
        QVERIFY(!stack.undo(paper).sheetChanged);
        QCOMPARE(paper.hash(), a);

        // Un trazo nuevo descarta lo rehacible de la pila.
        stack.insertAfterActive(paper, ++tag);
        QVERIFY(stack.undo(paper).sheetChanged);
        stroke(500);
        QVERIFY(!stack.redo(paper).sheetChanged);
        QCOMPARE(stack.count(), 2);
    }

    // HU-88: mover una hoja se deshace y se rehace, en orden con los trazos, sin cambiar la
    // hoja del papel ni el contenido de ninguna.
    void pilaMoverSeDeshace()
    {
        drymedia::Paper paper({.seed = 3, .widthMm = 80, .heightMm = 60});
        SheetStack stack(100);
        drymedia::Pencil pencil(paper, drymedia::Medium::hb());
        pencil.setTileObserver([&stack](int i, const uint16_t* before, uint8_t planes) {
            stack.history().beforeTileWrite(i, before, planes);
        });
        uint64_t tag = 0;
        const auto stroke = [&](double y) {
            stack.history().beginStroke(++tag);
            stack.clearRedo();
            pencil.beginStroke({200, y, 0.6f, 0, 80});
            for (int i = 1; i <= 30; ++i)
                pencil.strokeTo({200 + 1000.0 * i / 30, y, 0.6f, 0, 80});
            pencil.endStroke();
            stack.history().endStroke(paper);
        };
        stack.add(paper, 1, ++tag);
        stack.add(paper, 2, ++tag); // [1*, 2, 3]
        stack.activate(paper, 1);   // [1, 2*, 3]
        stroke(300);
        const uint64_t drawn = paper.hash();
        QVERIFY(!stack.move(1, 0, ++tag)); // [2*, 1, 3]: la activa sigue en el papel
        QCOMPARE(stack.order().ids, (std::vector<uint64_t>{2, 1, 3}));
        QCOMPARE(stack.active(), 0);
        QVERIFY(!stack.move(0, 0, ++tag)); // mismo lugar: nada, y no se guarda
        stroke(500);                       // después del movimiento
        const uint64_t twice = paper.hash();

        SheetStack::UndoResult r = stack.undo(paper); // el trazo
        QVERIFY(!r.sheetChanged && !r.tiles.empty());
        r = stack.undo(paper); // el movimiento: sin repintar
        QVERIFY(!r.sheetChanged && r.tiles.empty());
        QCOMPARE(stack.order().ids, (std::vector<uint64_t>{1, 2, 3}));
        QCOMPARE(stack.active(), 1);
        QCOMPARE(paper.hash(), drawn);
        QVERIFY(!stack.redo(paper).sheetChanged); // el movimiento otra vez
        QCOMPARE(stack.order().ids, (std::vector<uint64_t>{2, 1, 3}));
        QCOMPARE(stack.active(), 0);
        stack.redo(paper); // y el trazo
        QCOMPARE(paper.hash(), twice);
    }

    // HU-87: en la simulación, Insert + un trazo + Z dos veces deja la pila como estaba; lo
    // publicado cuenta los pedidos atendidos.
    void simulacionDeshaceLaPila()
    {
        drymedia::Paper paper({.seed = 3, .widthMm = 80, .heightMm = 60});
        SheetStack stack(Simulation::kUndoLimit);
        SampleQueue queue;
        DisplayImage image;
        image.width = 400;
        image.height = 300;
        image.pixels.resize(size_t(image.width) * size_t(image.height));
        const SheetMapping mapping = SheetMapping::fit(image.width, image.height, paper.width(), paper.height());
        Simulation sim(paper, stack, queue, image, mapping);
        sim.renderAll();
        sim.start();

        sim.requestInsertSheet();
        QTRY_COMPARE(sim.sheetState().applied, uint64_t(1));
        QCOMPARE(sim.sheetCount(), 2);
        QCOMPARE(sim.activeSheet(), 1);
        QCOMPARE(sim.sheetState().activeTiles, size_t(0));

        std::vector<tabletinput::PenSample> samples;
        for (int i = 0; i <= 20; ++i) {
            tabletinput::PenSample s;
            s.x = mapping.sheetX + 50 + 10 * i;
            s.y = mapping.sheetY + 100;
            s.pressure = 0.6f;
            s.altitude = 80;
            s.inContact = true;
            s.timeUs = 1000 * (i + 1);
            samples.push_back(s);
        }
        samples.push_back(samples.back());
        samples.back().inContact = false;
        queue.push(samples);
        QTRY_VERIFY(sim.sheetState().activeTiles > 0);

        sim.requestUndo(); // el trazo
        sim.requestUndo(); // la hoja insertada
        QTRY_COMPARE(sim.sheetState().applied, uint64_t(3));
        QCOMPARE(sim.sheetCount(), 1);
        QCOMPARE(sim.activeSheet(), 0);

        // HU-88: mover la activa y Z la devuelve.
        sim.requestAddSheet(1);
        sim.requestMoveSheet(0, 1);
        QTRY_COMPARE(sim.sheetState().applied, uint64_t(5));
        QCOMPARE(sim.sheetState().order.ids, (std::vector<uint64_t>{3, 1}));
        QCOMPARE(sim.activeSheet(), 1);
        sim.requestUndo();
        QTRY_COMPARE(sim.sheetState().applied, uint64_t(6));
        QCOMPARE(sim.sheetState().order.ids, (std::vector<uint64_t>{1, 3}));
        QCOMPARE(sim.activeSheet(), 0);
        sim.stop();
        QCOMPARE(paper.hash(), paper.reliefHash());
    }

    // HU-90: un pedido de la pila se atiende entre las muestras que llegaron antes y las que
    // llegaron después, aunque vengan en el mismo lote: soltar la flecha y apoyar el lápiz
    // enseguida dibuja en la hoja nueva.
    void simulacionOrdenaPedidosYMuestras()
    {
        drymedia::Paper paper({.seed = 3, .widthMm = 80, .heightMm = 60});
        SheetStack stack(Simulation::kUndoLimit);
        SampleQueue queue;
        DisplayImage image;
        image.width = 400;
        image.height = 300;
        image.pixels.resize(size_t(image.width) * size_t(image.height));
        const SheetMapping mapping = SheetMapping::fit(image.width, image.height, paper.width(), paper.height());
        Simulation sim(paper, stack, queue, image, mapping);
        sim.renderAll();
        QCOMPARE(image.sheet, uint64_t(1));
        const auto stroke = [&](int y) {
            std::vector<tabletinput::PenSample> samples;
            for (int i = 0; i <= 20; ++i) {
                tabletinput::PenSample s;
                s.x = mapping.sheetX + 50 + 10 * i;
                s.y = mapping.sheetY + y;
                s.pressure = 0.6f;
                s.altitude = 80;
                s.inContact = true;
                s.timeUs = 1000 * (i + 1);
                samples.push_back(s);
            }
            samples.push_back(samples.back());
            samples.back().inContact = false;
            return samples;
        };

        // Todo antes de arrancar: llega en un solo lote.
        sim.requestAddSheet(1);
        queue.push(stroke(60)); // en la hoja 1, la activa
        sim.requestActivateSheet(1);
        queue.push(stroke(160)); // en la 2
        sim.start();
        QTRY_COMPARE(sim.sheetState().applied, uint64_t(2));
        sim.stop();

        QCOMPARE(sim.activeSheet(), 1);
        QVERIFY(stack.hash(paper, 0) != paper.reliefHash());
        QVERIFY(stack.hash(paper, 1) != paper.reliefHash());
        QCOMPARE(image.sheet, stack.order().activeId());
        // La 1 salió con su trazo ya en la imagen.
        QCOMPARE(image.retired.size(), size_t(1));
        const auto& left = image.retired[0].pixels;
        QVERIFY(std::any_of(left.begin(), left.end(), [](uint32_t px) { return px != kPaperColor; }));
    }

    // HU-98: con un solo grafito la dureza es la fija aunque config.json tenga otra; sin él, la
    // guardada, y si no hay, la HB.
    void durezaConLaQueArranca()
    {
        QCOMPARE(startingGrade(4, QStringLiteral("6B")), 4);
        QCOMPARE(startingGrade(-1, QStringLiteral("6B")), 9);
        QCOMPARE(startingGrade(-1, QString()), kHbIndex);
        QCOMPARE(startingGrade(-1, QStringLiteral("XX")), kHbIndex);
    }

    // HU-91: la mesa de luz se cuenta desde la activa; en los extremos, las hojas que haya.
    void mesaDeLuzDesdeLaActiva()
    {
        SheetOrder order;
        for (int i = 1; i < 5; ++i)
            order.add(i); // ids 1 a 5
        const LightTable table{{{0xA1, 0.6f}, {0xA2, 0.3f}}, {{0xB1, 0.6f}}};
        const auto ids = [&](const std::vector<SheetLayer>& layers) {
            std::vector<uint64_t> out;
            for (const SheetLayer& l : layers)
                out.push_back(l.sheet);
            return out;
        };

        order.activate(2); // la 3: dos atrás y una adelante
        std::vector<SheetLayer> layers = lightTableLayers(order, table);
        QCOMPARE(ids(layers), (std::vector<uint64_t>{1, 2, 4}));
        QCOMPARE(layers[0].color, 0xA2u); // la más lejana, más tenue
        QCOMPARE(layers[0].opacity, 0.3f);
        QCOMPARE(layers[1].color, 0xA1u);
        QCOMPARE(layers[2].color, 0xB1u);

        order.activate(0); // la primera: solo la siguiente
        QCOMPARE(ids(lightTableLayers(order, table)), (std::vector<uint64_t>{2}));
        order.activate(4); // la última: solo las anteriores
        QCOMPARE(ids(lightTableLayers(order, table)), (std::vector<uint64_t>{3, 4}));
        order.move(4, 0); // la activa pasa a ser la primera: se recalcula con el orden nuevo
        QCOMPARE(order.activeId(), uint64_t(5));
        QCOMPARE(ids(lightTableLayers(order, table)), (std::vector<uint64_t>{1}));
        QVERIFY(lightTableLayers(order, LightTable{}).empty()); // apagada
    }

    // HU-93: solo keys: las keys más cercanas a cada lado, con los colores de la más cercana a la
    // más lejana; la activa no cuenta aunque sea key; sin keys, nada.
    // HU-106: la franja del audio va en el hueco entre la hoja y el panel si entra; si no, sobre
    // el pie de la hoja. El tiempo, en minutos, segundos y décimas.
    void franjaDeAudio()
    {
        // Ultrawide: hoja de 1734 px, panel desde 2304: hueco de 570.
        QRect r = audioStripRect(2560, 1080, QRect(0, 0, 1734, 1080), 2304, 1);
        QCOMPARE(r, QRect(1734 + 24, 1080 - 16 - 130, 520, 130));
        // 16:9 sin hueco: abajo de la hoja, adentro.
        r = audioStripRect(1920, 1080, QRect(0, 0, 1728, 1080), 1664, 1);
        QCOMPARE(r, QRect(16, 1080 - 16 - 130, 520, 130));
        // Escala 125 %: todo más grande.
        QCOMPARE(audioStripRect(2560, 1080, QRect(0, 0, 1734, 1080), 2560, 1.25).height(), 163);
        QCOMPARE(audioWaveRect(QSize(520, 130), 1), QRect(16, 40, 488, 78));
        QCOMPARE(audioTime(0), QStringLiteral("0:00,0"));
        QCOMPARE(audioTime(3.24), QStringLiteral("0:03,2"));
        QCOMPARE(audioTime(72.05), QStringLiteral("1:12,1"));
        AudioStripInfo info{QStringLiteral("dialogo.wav"), {}, 12, 3, std::vector<float>(488, 0.5f)};
        const QImage strip = paintAudioStrip(info, QSize(520, 130), 1, Qt::black);
        QCOMPARE(strip.size(), QSize(520, 130));
        QVERIFY(strip.pixel(16 + 488 / 2, 40 + 39) != strip.pixel(16 + 488 / 2, 40 + 2)); // la onda, al medio
    }

    // HU-103: la guía de campo: 12 campos de 1 pulgada en 16:9, centrados; las líneas tienen
    // cobertura y entre ellas no.
    void guiaDeCampo()
    {
        constexpr int w = 1300, h = 800;
        constexpr double pxPerMm = 4; // el campo 12: 1219,2 × 685,8 px
        const QRectF twelve = fieldRect(12, w, h, pxPerMm);
        QCOMPARE(twelve.width(), 12 * 25.4 * pxPerMm);
        QVERIFY(std::abs(twelve.width() / twelve.height() - 16.0 / 9) < 1e-9);
        QCOMPARE(twelve.center(), QPointF(w / 2.0, h / 2.0));
        QCOMPARE(fieldRect(6, w, h, pxPerMm).width(), twelve.width() / 2);
        const QImage guide = paintFieldGuide(w, h, pxPerMm, 1);
        QCOMPARE(guide.size(), QSize(w, h));
        const auto alpha = [&](double x, double y) { return qAlpha(guide.pixel(int(x), int(y))); };
        QVERIFY(alpha(twelve.left(), h / 2.0 + 40) > 0);  // el borde del campo 12
        QVERIFY(alpha(w / 2.0, h / 2.0 + 40) > 0);        // la cruz
        QCOMPARE(alpha(5, 5), 0);                          // afuera
        const QRectF eleven = fieldRect(11, w, h, pxPerMm);
        QCOMPARE(alpha((twelve.left() + eleven.left()) / 2, h / 2.0 + 120), 0); // entre dos campos
    }

    // HU-89: la hoja fija no se mueve de la pila; el flip, los extremos, la numeración, la mesa
    // de luz y la tira del panel la saltean.
    void hojaFija()
    {
        SheetOrder o;
        for (int i = 1; i < 6; ++i)
            o.add(i); // ids 1 a 6
        const uint64_t fixed = 3; // la tercera
        QCOMPARE(fixedIndex(o, fixed), 2);
        QCOMPARE(fixedIndex(o, 0), -1);
        QCOMPARE(fixedIndex(o, 99), -1); // borrada: no cuenta
        QCOMPARE(flipStep(o, fixed, 1, 1), 3);
        QCOMPARE(flipStep(o, fixed, 3, -1), 1);
        QCOMPARE(flipStep(o, fixed, 5, 1), -1); // en el extremo
        QCOMPARE(flipStep(o, 99, 1, 1), 2);
        QCOMPARE(sheetNumber(o, fixed, 2), 0);
        QCOMPARE(sheetNumber(o, fixed, 1), 2);
        QCOMPARE(sheetNumber(o, fixed, 3), 3);
        QCOMPARE(sheetNumber(o, fixed, 5), 5);
        QCOMPARE(flipCount(o, fixed), 5);
        QCOMPARE(flipCount(o, 99), 6);
        QCOMPARE(flipEdge(o, 1, false), 1); // la fija es la primera
        QCOMPARE(flipEdge(o, 6, true), 4);  // la fija es la última
        QCOMPARE(flipEdge(o, fixed, true), 5);
        SheetOrder one;
        QCOMPARE(flipEdge(one, 1, false), -1); // la única es la fija

        // Mesa de luz: alrededor de la 4 (índice 3), la fija no es la anterior.
        o.activate(3);
        LightTable table{{{0xA1, 0.6f}, {0xA2, 0.3f}}, {{0xB1, 0.6f}}};
        std::vector<uint64_t> ids;
        for (const SheetLayer& l : lightTableLayers(o, table, {}, fixed))
            ids.push_back(l.sheet);
        QCOMPARE(ids, (std::vector<uint64_t>{1, 2, 5}));

        // Panel: la tira no la muestra y numera sin ella.
        const std::vector<PanelSlot> strip = panelSlots(o, 3, 3, {}, fixed);
        QCOMPARE(strip.size(), size_t(7));
        QCOMPARE(strip[0].id, uint64_t(0));
        QCOMPARE(strip[1].id, uint64_t(1));
        QCOMPARE(strip[2].id, uint64_t(2));
        QVERIFY(strip[3].center && strip[3].id == 4 && strip[3].number == 3);
        QCOMPARE(strip[4].id, uint64_t(5));
        QCOMPARE(strip[5].number, 5);
        QCOMPARE(strip[6].id, uint64_t(0));
    }

    // HU-94: con el círculo, el flip, la mesa de luz y la tira del panel dan la vuelta, sin
    // repetir hojas y salteando la fija.
    void pilaEnCirculo()
    {
        SheetOrder o;
        for (int i = 1; i < 4; ++i)
            o.add(i); // ids 1 a 4
        QCOMPARE(flipStep(o, 0, 3, 1), -1);
        QCOMPARE(flipStep(o, 0, 3, 1, true), 0);
        QCOMPARE(flipStep(o, 0, 0, -1, true), 3);
        QCOMPARE(flipStep(o, 1, 3, 1, true), 1); // la fija es la primera: se saltea
        SheetOrder one;
        QCOMPARE(flipStep(one, 0, 0, 1, true), -1); // una sola hoja: no hay otra

        const auto ids = [](const std::vector<SheetLayer>& layers) {
            std::vector<uint64_t> out;
            for (const SheetLayer& l : layers)
                out.push_back(l.sheet);
            return out;
        };
        const LightTable table{{{0xA1, 0.6f}, {0xA2, 0.3f}}, {{0xB1, 0.6f}}};
        o.activate(0); // la primera: atrás vienen la 4 y la 3
        QCOMPARE(ids(lightTableLayers(o, table, {}, 0, true)), (std::vector<uint64_t>{3, 4, 2}));
        QCOMPARE(ids(lightTableLayers(o, table, {}, 0, false)), (std::vector<uint64_t>{2}));
        SheetOrder two;
        two.add(1); // 2 hojas: atrás la otra; adelante no se repite
        QCOMPARE(ids(lightTableLayers(two, table, {}, 0, true)), (std::vector<uint64_t>{2}));

        o.activate(3); // la última
        const std::vector<PanelSlot> strip = panelSlots(o, 3, 3, {}, 0, true);
        QCOMPARE(strip[3].id, uint64_t(4));
        QCOMPARE(strip[4].id, uint64_t(1)); // después de la última, la primera
        QCOMPARE(strip[4].number, 1);
        QCOMPARE(strip[2].id, uint64_t(3));
        QCOMPARE(strip[1].id, uint64_t(2));
        QCOMPARE(strip[5].id, uint64_t(0)); // la 2 ya está atrás: no se repite
        QCOMPARE(strip[0].id, uint64_t(0));
    }

    void mesaDeLuzSoloKeys()
    {
        SheetOrder order;
        for (int i = 1; i < 8; ++i)
            order.add(i); // ids 1 a 8
        LightTable table{{{0xA1, 0.6f}, {0xA2, 0.3f}}, {{0xB1, 0.6f}}, true};
        const auto ids = [&](const std::vector<SheetLayer>& layers) {
            std::vector<uint64_t> out;
            for (const SheetLayer& l : layers)
                out.push_back(l.sheet);
            return out;
        };
        QVERIFY(lightTableLayers(order, table, {}).empty()); // sin keys

        const SheetRoles roles{{1, SheetRole::Key}, {3, SheetRole::Key}, {4, SheetRole::Breakdown}, {5, SheetRole::Key},
                               {8, SheetRole::Key}};
        order.activate(5); // la 6, entre la key 5 y la 8; el breakdown 4 no cuenta
        std::vector<SheetLayer> layers = lightTableLayers(order, table, roles);
        QCOMPARE(ids(layers), (std::vector<uint64_t>{3, 5, 8}));
        QCOMPARE(layers[0].color, 0xA2u); // la key más lejana, más tenue
        QCOMPARE(layers[1].color, 0xA1u);
        QCOMPARE(layers[2].color, 0xB1u);

        order.activate(4); // la activa es la key 5: cuentan las otras
        QCOMPARE(ids(lightTableLayers(order, table, roles)), (std::vector<uint64_t>{1, 3, 8}));
        order.activate(0); // la primera: solo hacia adelante
        QCOMPARE(ids(lightTableLayers(order, table, roles)), (std::vector<uint64_t>{3}));
        order.activate(7); // la última (key): la 5 y la 3 hacia atrás
        QCOMPARE(ids(lightTableLayers(order, table, roles)), (std::vector<uint64_t>{3, 5}));
        table.keysOnly = false; // sin el modo, las vecinas de siempre
        QCOMPARE(ids(lightTableLayers(order, table, roles)), (std::vector<uint64_t>{6, 7}));
    }

    // HU-90: la región de la hoja sale de la imagen de pantalla cuando ya tiene entera la
    // hoja pedida; si no (flip, o la imagen a medio subir después de soltar), de la textura
    // de esa hoja, o en blanco si nunca estuvo activa.
    void fuenteDeLaHoja()
    {
        SheetSource s;
        s.latched(1, true, true); // al arrancar: la 1, a medio subir
        QCOMPARE(s.uploaded(), uint64_t(0));
        QCOMPARE(s.pick(0, false), SheetSource::Image);
        QCOMPARE(s.pick(1, false), SheetSource::Image); // sin textura: mejor a medio subir
        QCOMPARE(s.pick(1, true), SheetSource::Texture);
        s.latched(1, true, false);
        QCOMPARE(s.uploaded(), uint64_t(1));
        QCOMPARE(s.pick(1, true), SheetSource::Image);
        QCOMPARE(s.pick(2, true), SheetSource::Texture); // flip a la 2
        QCOMPARE(s.pick(3, false), SheetSource::Blank);
        s.latched(1, true, false); // un trazo en la 1: sigue entera
        s.latched(1, false, false);
        QCOMPARE(s.uploaded(), uint64_t(1));

        s.latched(2, true, true); // se soltó en la 2: la imagen empieza a ser la 2
        QCOMPARE(s.uploaded(), uint64_t(0));
        QCOMPARE(s.pick(2, true), SheetSource::Texture);
        QCOMPARE(s.pick(1, true), SheetSource::Texture); // la imagen ya no es la 1
        s.latched(2, true, false);
        QCOMPARE(s.pick(2, true), SheetSource::Image);
    }

    // HU-82: la simulación atiende la pila en su hilo y el trazo cae en la hoja activa.
    void simulacionAtiendeLaPila()
    {
        drymedia::Paper paper({.seed = 3, .widthMm = 80, .heightMm = 60});
        SheetStack stack(Simulation::kUndoLimit);
        SampleQueue queue;
        DisplayImage image;
        image.width = 400;
        image.height = 300;
        image.pixels.resize(size_t(image.width) * size_t(image.height));
        const SheetMapping mapping = SheetMapping::fit(image.width, image.height, paper.width(), paper.height());
        Simulation sim(paper, stack, queue, image, mapping);
        sim.renderAll();
        sim.start();

        sim.requestAddSheet(1);
        sim.requestActivateSheet(1);
        QTRY_COMPARE(sim.activeSheet(), 1);
        QCOMPARE(sim.sheetCount(), 2);
        QVERIFY(sim.lastActivationMs() > 0);

        std::vector<tabletinput::PenSample> samples;
        for (int i = 0; i <= 20; ++i) {
            tabletinput::PenSample s;
            s.x = mapping.sheetX + 50 + 10 * i;
            s.y = mapping.sheetY + 100;
            s.pressure = 0.6f;
            s.altitude = 80;
            s.inContact = true;
            s.timeUs = 1000 * (i + 1);
            samples.push_back(s);
        }
        samples.push_back(samples.back());
        samples.back().inContact = false;
        queue.push(samples);
        sim.requestActivateSheet(0); // después de las muestras: el trazo queda en la 1
        QTRY_COMPARE(sim.activeSheet(), 0);
        sim.stop();

        QCOMPARE(stack.hash(paper, 0), paper.reliefHash());
        QVERIFY(stack.hash(paper, 1) != paper.reliefHash());

        // HU-83: cada hoja que salió dejó su imagen para el render (la 1 al activar la 2, y la
        // 2, con el trazo, al volver a la 1), del tamaño de la hoja.
        QCOMPARE(image.retired.size(), size_t(2));
        QCOMPARE(image.retired[0].sheet, uint64_t(1));
        QCOMPARE(image.retired[1].sheet, uint64_t(2));
        QCOMPARE(image.retired[1].width, mapping.sheetWidth);
        QCOMPARE(image.retired[1].pixels.size(), size_t(mapping.sheetWidth) * size_t(mapping.sheetHeight));
        const auto& drawn = image.retired[1].pixels;
        QVERIFY(std::any_of(drawn.begin(), drawn.end(), [](uint32_t px) { return px != kPaperColor; }));
        const auto& blankSheet = image.retired[0].pixels;
        QVERIFY(std::all_of(blankSheet.begin(), blankSheet.end(), [](uint32_t px) { return px == kPaperColor; }));
    }

    // HU-85: una toma guardada y abierta es la misma pila: mismo orden, misma activa, el mismo
    // hash en cada hoja, la vista y las imágenes guardadas. Guardar de nuevo borra los archivos
    // de las hojas que ya no están; una toma de otro papel no se abre.
    void tomaGuardadaYAbierta()
    {
        const drymedia::PaperSpec spec{.seed = 3, .widthMm = 80, .heightMm = 60};
        drymedia::Paper paper(spec);
        SheetStack stack(100);
        drymedia::Pencil pencil(paper, drymedia::Medium::hb());
        const auto stroke = [&](double y) {
            pencil.beginStroke({200, y, 0.6f, 0, 80});
            for (int i = 1; i <= 30; ++i)
                pencil.strokeTo({200 + 1000.0 * i / 30, y, 0.6f, 0, 80});
            pencil.endStroke();
        };
        stroke(300);
        stack.add(paper, 1);
        stack.add(paper, 2);
        stack.activate(paper, 2);
        stroke(800);
        stack.activate(paper, 1); // la activa queda en el medio, con su trazo
        stroke(500);
        std::vector<uint64_t> hashes;
        for (int i = 0; i < 3; ++i)
            hashes.push_back(stack.hash(paper, i));
        const std::vector<uint64_t> ids = stack.order().ids;
        const std::vector<uint32_t> pixels{1, 2, 3, 4, 5, 6};
        const PackedImage image = PackedImage::pack(3, 2, pixels.data());

        QTemporaryDir dir;
        const TakeView roles{.degrees = 45.0, .zoom = 2.0, .panX = 0.1, .panY = -0.25, // HU-101, HU-102
                             .roles = {{ids[0], SheetRole::Key}, {ids[2], SheetRole::Breakdown}}, // HU-104
                             .fixed = ids[2], // HU-89
                             .circle = true, // HU-94
                             .audio = QStringLiteral("audio.wav")}; // HU-106
        const TakeResult saved = saveTake(dir.path(), paper, stack, roles,
                                          [&](uint64_t id) { return id == ids[0] ? image : PackedImage{}; });
        QVERIFY2(saved.ok, qPrintable(saved.error));
        const QDir folder(dir.path());
        QCOMPARE(folder.entryList({QStringLiteral("hoja-*.hoja")}, QDir::Files).size(), 3);
        QFile json(folder.filePath(QStringLiteral("toma.json")));
        QVERIFY(json.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(json.readAll()).object();
        json.close();
        QCOMPARE(root.value(QStringLiteral("version")).toInt(), kTakeVersion);
        QCOMPARE(root.value(QStringLiteral("hojas")).toArray().size(), 3);
        QCOMPARE(root.value(QStringLiteral("activa")).toInt(), 1);
        QCOMPARE(root.value(QStringLiteral("hojas")).toArray()[0].toObject().value(QStringLiteral("rol")).toString(),
                 QStringLiteral("key"));
        QCOMPARE(root.value(QStringLiteral("hojas")).toArray()[1].toObject().value(QStringLiteral("rol")).toString(), QString());
        QVERIFY(root.value(QStringLiteral("hojas")).toArray()[2].toObject().value(QStringLiteral("fija")).toBool());
        QVERIFY(!root.value(QStringLiteral("hojas")).toArray()[0].toObject().contains(QStringLiteral("fija")));

        drymedia::Paper other(spec);
        SheetStack opened(100);
        TakeView view;
        std::vector<std::pair<uint64_t, PackedImage>> images;
        const TakeResult result = openTake(dir.path(), other, opened, view, &images);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.blank.empty());
        QCOMPARE(opened.order().ids, ids);
        QCOMPARE(opened.active(), 1);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(opened.hash(other, i), hashes[size_t(i)]);
        QCOMPARE(view.degrees, 45.0);
        QVERIFY(view.roles == roles.roles);
        QCOMPARE(view.fixed, ids[2]);
        QVERIFY(view.circle);
        QCOMPARE(view.audio, QStringLiteral("audio.wav"));
        QCOMPARE(root.value(QStringLiteral("audio")).toObject().value(QStringLiteral("archivo")).toString(),
                 QStringLiteral("audio.wav"));
        QCOMPARE(view.zoom, 2.0);
        QCOMPARE(view.panX, 0.1);
        QCOMPARE(view.panY, -0.25);
        QCOMPARE(images.size(), size_t(1));
        QCOMPARE(images[0].first, ids[0]);
        std::vector<uint32_t> back;
        QVERIFY(images[0].second.unpack(back));
        QCOMPARE(back, pixels);

        // Los ids nuevos siguen después de los de la toma; borrar una hoja borra su archivo.
        opened.add(other, 3);
        QCOMPARE(opened.order().ids.back(), *std::max_element(ids.begin(), ids.end()) + 1);
        opened.remove(other, 0);
        QVERIFY(saveTake(dir.path(), other, opened, {}, [](uint64_t) { return PackedImage{}; }).ok);
        QVERIFY(!folder.exists(QStringLiteral("hoja-%1.hoja").arg(ids[0])));
        QCOMPARE(folder.entryList({QStringLiteral("hoja-*.hoja")}, QDir::Files).size(), 3);

        // Otro papel: no abre y la pila queda como estaba.
        drymedia::Paper rough({.seed = 3, .widthMm = 80, .heightMm = 60, .grain = 900});
        SheetStack untouched(100);
        TakeView unused;
        const TakeResult refused = openTake(dir.path(), rough, untouched, unused);
        QVERIFY(!refused.ok);
        QCOMPARE(untouched.count(), 1);
        QVERIFY(!openTake(folder.filePath(QStringLiteral("no-existe")), other, opened, unused).ok);
    }

    // HU-85: un archivo de hoja dañado, cortado o que falta no impide abrir la toma: esa hoja
    // queda en blanco (y se dice cuál y por qué); las demás, intactas.
    void tomaConHojasDanadas()
    {
        const drymedia::PaperSpec spec{.seed = 4, .widthMm = 60, .heightMm = 40};
        drymedia::Paper paper(spec);
        SheetStack stack(100);
        drymedia::Pencil pencil(paper, drymedia::Medium::hb());
        for (int i = 0; i < 4; ++i) {
            if (i > 0) {
                stack.add(paper, i);
                stack.activate(paper, i);
            }
            pencil.beginStroke({100, 100.0 + 150 * i, 0.6f, 0, 80});
            for (int k = 1; k <= 20; ++k)
                pencil.strokeTo({100 + 600.0 * k / 20, 100.0 + 150 * i, 0.6f, 0, 80});
            pencil.endStroke();
        }
        const uint64_t last = stack.hash(paper, 3);
        const std::vector<uint64_t> ids = stack.order().ids;
        QTemporaryDir dir;
        QVERIFY(saveTake(dir.path(), paper, stack, {}, [](uint64_t) { return PackedImage{}; }).ok);
        const QDir folder(dir.path());
        const auto path = [&](int i) { return folder.filePath(QStringLiteral("hoja-%1.hoja").arg(ids[size_t(i)])); };
        {
            QFile damaged(path(0)); // un byte cambiado en el depósito comprimido
            QVERIFY(damaged.open(QIODevice::ReadWrite));
            QByteArray bytes = damaged.readAll();
            bytes[bytes.size() / 2] = char(bytes[bytes.size() / 2] ^ 0x5A);
            damaged.seek(0);
            damaged.write(bytes);
        }
        {
            QFile cut(path(1));
            QVERIFY(cut.resize(cut.size() / 2));
        }
        QVERIFY(QFile::remove(path(2)));

        drymedia::Paper other(spec);
        SheetStack opened(100);
        TakeView view;
        const TakeResult result = openTake(dir.path(), other, opened, view);
        QVERIFY(result.ok);
        QCOMPARE(opened.count(), 4);
        QCOMPARE(result.blank.size(), size_t(3));
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(result.blank[size_t(i)].first, i);
            QVERIFY(!result.blank[size_t(i)].second.isEmpty());
            QCOMPARE(opened.hash(other, i), other.reliefHash()); // en blanco
        }
        QCOMPARE(opened.hash(other, 3), last);
    }

    // HU-85: las imágenes de las hojas se comprimen en su hilo y se recuperan iguales.
    void imagenesDeLasHojas()
    {
        SheetImages images;
        std::vector<uint32_t> pixels(64 * 48);
        for (size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = uint32_t(i * 2654435761u);
        images.put(7, 64, 48, pixels);
        const PackedImage packed = images.get(7);
        QCOMPARE(packed.width, 64);
        QCOMPARE(packed.height, 48);
        std::vector<uint32_t> back;
        QVERIFY(packed.unpack(back));
        QCOMPARE(back, pixels);
        QVERIFY(images.get(8).empty());
        PackedImage broken = packed;
        broken.bytes.resize(broken.bytes.size() / 2);
        QVERIFY(!broken.unpack(back));
        images.clear();
        QVERIFY(images.get(7).empty());
    }

    // HU-114: el panel de hojas muestra la activa y 3 antes y después; afuera de la pila, lugares
    // vacíos.
    void panelLugares()
    {
        SheetOrder o;
        for (int i = 0; i < 5; ++i)
            o.add(o.count); // 6 hojas, ids 1..6
        auto strip = panelSlots(o, 1);
        QCOMPARE(strip.size(), size_t(7));
        QCOMPARE(strip[0].id, uint64_t(0)); // antes de la primera
        QCOMPARE(strip[1].id, uint64_t(0));
        QCOMPARE(strip[2].id, uint64_t(1));
        QCOMPARE(strip[2].number, 1);
        QVERIFY(strip[3].center && strip[3].number == 2);
        QCOMPARE(strip[6].number, 5);
        strip = panelSlots(o, 5);
        QCOMPARE(strip[3].number, 6);
        QCOMPARE(strip[4].id, uint64_t(0)); // después de la última
    }

    // HU-104: K da la vuelta sin rol → key → breakdown; el rol sigue a su hoja al reordenar y
    // el número es la posición.
    void rolesDeLaHoja()
    {
        QCOMPARE(nextRole(SheetRole::None), SheetRole::Key);
        QCOMPARE(nextRole(SheetRole::Key), SheetRole::Breakdown);
        QCOMPARE(nextRole(SheetRole::Breakdown), SheetRole::None);
        for (SheetRole r : {SheetRole::None, SheetRole::Key, SheetRole::Breakdown})
            QCOMPARE(parseRole(roleName(r)), r);
        QCOMPARE(parseRole(QStringLiteral("otra cosa")), SheetRole::None);

        SheetOrder o;
        o.add(1);
        o.add(2); // ids 1, 2, 3
        const SheetRoles roles{{1, SheetRole::Key}, {3, SheetRole::Breakdown}};
        QCOMPARE(roleOf(roles, 2), SheetRole::None);
        o.move(0, 2); // la key pasa al final: 2, 3, 1
        const std::vector<PanelSlot> strip = panelSlots(o, 2, 1, roles);
        QCOMPARE(strip[1].id, uint64_t(1));
        QCOMPARE(strip[1].number, 3);
        QCOMPARE(strip[1].role, SheetRole::Key);
        QCOMPARE(strip[0].role, SheetRole::Breakdown);
        QCOMPARE(strip[0].number, 2);
    }

    // HU-104: la marca de la esquina: el círculo solo en la key, la raya solo en el breakdown.
    void marcaDeLaHoja()
    {
        const QImage none = paintSheetMark(12, SheetRole::None, 64);
        const QImage key = paintSheetMark(12, SheetRole::Key, 64);
        const QImage breakdown = paintSheetMark(12, SheetRole::Breakdown, 64);
        QCOMPARE(key.size(), QSize(64, 64));
        const auto ink = [](const QImage& im, int x, int y) { return qAlpha(im.pixel(x, y)) > 64; };
        QVERIFY(ink(key, 9, 32));   // el borde izquierdo del círculo
        QVERIFY(!ink(none, 9, 32));
        QVERIFY(!ink(breakdown, 9, 32));
        bool line = false, lineNone = false;
        for (int y = 44; y < 56; ++y) { // debajo de los números
            line = line || ink(breakdown, 32, y);
            lineNone = lineNone || ink(none, 32, y);
        }
        QVERIFY(line);
        QVERIFY(!lineNone);
    }

    // HU-114: el panel va pegado a la derecha, de 240 px a escala 1; las miniaturas tienen la
    // forma de la hoja, entran en el panel y se achican en una pantalla baja.
    void panelMedidas()
    {
        const double aspect = 1734.0 / 1081.0;
        const PanelLayout big = panelLayout(2560, 1080, 1.0, aspect);
        QCOMPARE(big.panel, QRect(2560 - 16 - 240, 16, 240, 1048));
        QCOMPARE(big.thumbs.size(), size_t(7));
        QCOMPARE(big.rowHeight, 84);
        for (const QRect& t : big.thumbs) {
            QVERIFY(QRect(QPoint(0, 0), big.panel.size()).contains(t));
            QVERIFY(std::abs(double(t.width()) / t.height() - aspect) < 0.05);
        }
        QVERIFY(big.thumbs[1].top() > big.thumbs[0].bottom());
        // Laptop: 1920 × 1080 físicos al 125 %: el panel es más ancho en píxeles y las
        // miniaturas, más chicas, para que entre todo.
        const PanelLayout laptop = panelLayout(1920, 1080, 1.25, aspect);
        QCOMPARE(laptop.panel.width(), 300);
        QVERIFY(laptop.rowHeight < 84 * 1.25);
        QVERIFY(laptop.thumbs.back().bottom() < laptop.panel.height() - 200); // queda lugar para el resto
    }

    // HU-114: el panel pintado: del tamaño de su lugar; la activa con marco de acento, las
    // demás con el papel adentro hasta que el render ponga la miniatura; afuera de la pila, nada.
    void panelPintado()
    {
        SheetOrder o;
        o.add(1);
        PanelInfo info;
        info.take = QStringLiteral("toma-1");
        info.number = 1;
        info.total = 2;
        info.strip = panelSlots(o, 0);
        info.lead = QStringLiteral("HB · 0,87 mm");
        const PanelLayout layout = panelLayout(2560, 1080, 1.0, 1.6);
        const QColor paper(0xFA, 0xF8, 0xF2);
        const QImage image = paintPanel(info, layout, paper, QColor(0x30, 0x33, 0x38));
        QCOMPARE(image.size(), layout.panel.size());
        const QRect active = layout.thumbs[3];
        QCOMPARE(QColor(image.pixel(active.center())), paper);
        QCOMPARE(QColor(image.pixel(active.left() - 1, active.center().y())), appkit::theme::kSeleccionTexto);
        const QRect next = layout.thumbs[4];
        QCOMPARE(QColor(image.pixel(next.center())), paper);
        QCOMPARE(QColor(image.pixel(next.left() - 2, next.center().y())), appkit::theme::kBorde);
        const QRect empty = layout.thumbs[0];
        QCOMPARE(QColor(image.pixel(empty.center())), appkit::theme::kPanel);
        QCOMPARE(QColor(image.pixel(0, 0)), QColor(0x30, 0x33, 0x38)); // la esquina redondeada
    }

    // HU-86: una toma que no se abre no se pisa: se usa la de al lado que abre, o una nueva;
    // arrancar otra vez vuelve a la misma (no crea una carpeta por arranque).
    void carpetaDeLaToma()
    {
        QTemporaryDir root;
        const QString dir = QDir(root.path()).filePath(QStringLiteral("toma-1"));
        const auto takeIn = [](const QString& folder) {
            QDir().mkpath(folder);
            QFile file(QDir(folder).filePath(QStringLiteral("toma.json")));
            QVERIFY(file.open(QIODevice::WriteOnly));
        };
        QStringList openable;
        const auto open = [&](const QString& folder) { return openable.contains(folder); };
        QCOMPARE(takeFolder(dir, open), dir); // no hay toma: una nueva ahí
        takeIn(dir);
        openable << dir;
        QCOMPARE(takeFolder(dir, open), dir); // abre
        openable.clear();
        QCOMPARE(takeFolder(dir, open), dir + QStringLiteral("-2")); // no abre: la de al lado, nueva
        takeIn(dir + QStringLiteral("-2"));
        openable << dir + QStringLiteral("-2");
        QCOMPARE(takeFolder(dir, open), dir + QStringLiteral("-2")); // la de al lado ya existe y abre
        openable.clear();
        QCOMPARE(takeFolder(dir, open), dir + QStringLiteral("-3"));
    }

    // HU-86: un trazo en la simulación, de y a la hoja de 400 × 300 px de las pruebas.
    static std::vector<tabletinput::PenSample> penStroke(const SheetMapping& mapping, int y)
    {
        std::vector<tabletinput::PenSample> samples;
        for (int i = 0; i <= 20; ++i) {
            tabletinput::PenSample s;
            s.x = mapping.sheetX + 50 + 10 * i;
            s.y = mapping.sheetY + y;
            s.pressure = 0.6f;
            s.altitude = 80;
            s.inContact = true;
            s.timeUs = 1000 * (i + 1);
            samples.push_back(s);
        }
        samples.push_back(samples.back());
        samples.back().inContact = false;
        return samples;
    }

    // HU-86: con la simulación andando, la toma se guarda sola: una toma nueva aparece sin
    // dibujar nada; una hoja insertada tiene su archivo enseguida; borrar una hoja borra el suyo
    // y deshacerlo lo vuelve a escribir; activar otra hoja reescribe toma.json pero ninguna
    // hoja. Lo guardado se abre igual a la pila.
    void guardadoAutomatico()
    {
        const drymedia::PaperSpec spec{.seed = 5, .widthMm = 80, .heightMm = 60};
        drymedia::Paper paper(spec);
        SheetStack stack(Simulation::kUndoLimit);
        SampleQueue queue;
        DisplayImage image;
        image.width = 400;
        image.height = 300;
        image.pixels.resize(size_t(image.width) * size_t(image.height));
        const SheetMapping mapping = SheetMapping::fit(image.width, image.height, paper.width(), paper.height());
        QTemporaryDir dir;
        const QDir folder(dir.path());
        SheetImages images;
        TakeSaver saver(dir.path(), paper, &images);
        const auto files = [&] {
            saver.flush();
            return folder.entryList({QStringLiteral("hoja-*.hoja")}, QDir::Files);
        };
        auto sim = std::make_unique<Simulation>(paper, stack, queue, image, mapping);
        sim->setImages(&images);
        sim->setAutosave(&saver, 0);
        sim->renderAll();
        sim->start();

        QTRY_COMPARE(files().size(), 1); // la hoja en blanco de la toma nueva
        QTRY_VERIFY(folder.exists(QStringLiteral("toma.json")));
        queue.push(penStroke(mapping, 100));
        sim->requestInsertSheet();
        QTRY_COMPARE(files().size(), 2);
        queue.push(penStroke(mapping, 150)); // en la 2
        sim->requestRemoveSheet(0);
        QTRY_COMPARE(files(), QStringList{QStringLiteral("hoja-2.hoja")});
        sim->requestUndo();
        QTRY_COMPARE(files().size(), 2);
        QTRY_COMPARE(sim->sheetState().applied, uint64_t(3));
        sim->stop();
        saver.flush();

        const auto opened = [&](SheetStack& into, drymedia::Paper& other) {
            TakeView view;
            QVERIFY(openTake(dir.path(), other, into, view).ok);
        };
        {
            drymedia::Paper other(spec);
            SheetStack back(100);
            opened(back, other);
            QCOMPARE(back.order().ids, stack.order().ids);
            QCOMPARE(back.active(), stack.active());
            for (int i = 0; i < 2; ++i) {
                QCOMPARE(back.hash(other, i), stack.hash(paper, i));
                QVERIFY(back.hash(other, i) != other.reliefHash());
            }
        }

        // Activar otra hoja: toma.json sí, las hojas no (no cambiaron).
        const TakeSaver::Stats before = saver.stats();
        sim = std::make_unique<Simulation>(paper, stack, queue, image, mapping);
        sim->setImages(&images);
        sim->setAutosave(&saver, 0);
        sim->renderAll();
        sim->start();
        sim->requestActivateSheet(0);
        QTRY_COMPARE(sim->sheetState().applied, uint64_t(1));
        QTRY_VERIFY(saver.stats().indexes > before.indexes);
        sim->stop();
        saver.flush();
        QCOMPARE(saver.stats().sheets, before.sheets);
        QVERIFY(saver.stats().error.isEmpty());
        drymedia::Paper other(spec);
        SheetStack back(100);
        opened(back, other);
        QCOMPARE(back.active(), 0);
    }

    // HU-86: con una espera larga no se guarda mientras se dibuja; al detener la simulación
    // (cerrar la app) se guarda lo pendiente. Dos copias de una hoja que no llegó a escribirse:
    // queda la última.
    void guardadoAlDetenerse()
    {
        const drymedia::PaperSpec spec{.seed = 6, .widthMm = 60, .heightMm = 40};
        drymedia::Paper paper(spec);
        SheetStack stack(Simulation::kUndoLimit);
        SampleQueue queue;
        DisplayImage image;
        image.width = 400;
        image.height = 300;
        image.pixels.resize(size_t(image.width) * size_t(image.height));
        const SheetMapping mapping = SheetMapping::fit(image.width, image.height, paper.width(), paper.height());
        QTemporaryDir dir;
        const QDir folder(dir.path());
        TakeSaver saver(dir.path(), paper, nullptr);
        Simulation sim(paper, stack, queue, image, mapping);
        sim.setAutosave(&saver, 60000);
        sim.renderAll();
        sim.start();
        queue.push(penStroke(mapping, 100));
        QTRY_VERIFY(sim.sheetState().activeTiles > 0);
        QTest::qWait(200);
        saver.flush();
        QVERIFY(!folder.exists(QStringLiteral("toma.json")));
        sim.stop();
        saver.flush();
        QVERIFY(folder.exists(QStringLiteral("hoja-1.hoja")));
        drymedia::Paper other(spec);
        SheetStack back(100);
        TakeView view;
        QVERIFY(openTake(dir.path(), other, back, view).ok);
        QCOMPARE(back.hash(other, 0), stack.hash(paper, 0));

        // La última copia gana: la hoja dibujada y después en blanco.
        const drymedia::TileSet empty = paper.newTileSet();
        saver.saveSheet(1, paper.serializeTiles(), 0, 0, {});
        saver.saveSheet(1, paper.serializeTiles(&empty), 0, 0, {});
        saver.flush();
        SheetStack again(100);
        QVERIFY(openTake(dir.path(), other, again, view).ok);
        QCOMPARE(again.hash(other, 0), other.reliefHash());
    }
};

QTEST_MAIN(TestLienzo)
#include "tst_lienzo.moc"
