#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <QElapsedTimer>
#include <QTest>

#include <cmath>
#include <functional>
#include <numbers>

using namespace drymedia;

namespace {

PaperSpec smallSheet()
{
    return {.seed = 5, .widthMm = 80, .heightMm = 60};
}

// Camino paramétrico t ∈ [0, 1] → muestra (celdas). Presión que varía suave.
using Path = std::function<PencilSample(double)>;

PencilSample lineAt(double t)
{
    return {200 + t * 1200, 300 + t * 500, float(0.4 + 0.3 * std::sin(t * 6)), 30.0f, 70.0f};
}

PencilSample curveAt(double t)
{
    const double a = t * std::numbers::pi * 1.5;
    return {900 + 600 * std::cos(a), 700 + 400 * std::sin(a), float(0.5 + 0.2 * std::sin(t * 9)),
            float(a * 180 / std::numbers::pi), 60.0f};
}

void draw(Pencil& pencil, const Path& path, int samples)
{
    pencil.beginStroke(path(0));
    for (int i = 1; i <= samples; ++i)
        pencil.strokeTo(path(double(i) / samples));
    pencil.endStroke();
}

// Depósito total y por celda (en un arreglo denso del tamaño de la hoja).
std::vector<uint16_t> depositOf(const Paper& paper)
{
    std::vector<uint16_t> all(size_t(paper.width()) * size_t(paper.height()), 0);
    for (int ty = 0; ty < paper.tilesY(); ++ty)
        for (int tx = 0; tx < paper.tilesX(); ++tx) {
            const uint16_t* tile = paper.findDepositTile(tx, ty);
            if (!tile)
                continue;
            for (int y = 0; y < kTileSize; ++y)
                for (int x = 0; x < kTileSize; ++x) {
                    const int gx = tx * kTileSize + x, gy = ty * kTileSize + y;
                    if (gx < paper.width() && gy < paper.height())
                        all[size_t(gy) * size_t(paper.width()) + size_t(gx)] = tile[y * kTileSize + x];
                }
        }
    return all;
}

double total(const std::vector<uint16_t>& v)
{
    double s = 0;
    for (const uint16_t d : v)
        s += d;
    return s;
}

// Compara el mismo camino con N y con 3N muestras: diferencia relativa del total y
// diferencia media por celda (sobre las celdas con depósito) relativa al depósito medio.
void continuity(const Path& path, int samples, double& totalDiff, double& cellDiff)
{
    Paper a(smallSheet()), b(smallSheet());
    Pencil pa(a, Medium::hb()), pb(b, Medium::hb());
    draw(pa, path, samples);
    draw(pb, path, samples * 3);
    const std::vector<uint16_t> da = depositOf(a), db = depositOf(b);
    const double ta = total(da), tb = total(db);
    totalDiff = std::abs(ta - tb) / ta;
    double diff = 0;
    int cells = 0;
    for (size_t i = 0; i < da.size(); ++i)
        if (da[i] || db[i]) {
            diff += std::abs(double(da[i]) - double(db[i]));
            ++cells;
        }
    cellDiff = (diff / cells) / (ta / cells);
}

} // namespace

class TestPencil : public QObject {
    Q_OBJECT

private slots:
    void elTrazoDeposita()
    {
        Paper paper(smallSheet());
        Pencil pencil(paper, Medium::hb());
        draw(pencil, lineAt, 40);
        QVERIFY(total(depositOf(paper)) > 0);
        QVERIFY(!pencil.strokeTiles().empty());
        QCOMPARE(paper.tileCount(), pencil.strokeTiles().size());
    }

    // El criterio de "sin dabs": el resultado casi no depende de cuántas muestras tenga
    // el trazo. En una recta las muestras extra caen sobre el mismo segmento; en una curva,
    // sobre el arco (las cuerdas se acortan), así que la tolerancia es un poco mayor.
    void continuidadRecta()
    {
        double totalDiff, cellDiff;
        continuity(lineAt, 40, totalDiff, cellDiff);
        qInfo() << "recta 40 vs 120 muestras: total" << totalDiff * 100 << "% · por celda" << cellDiff * 100 << "%";
        QVERIFY2(totalDiff < 0.03, qPrintable(QString::number(totalDiff)));
        QVERIFY2(cellDiff < 0.15, qPrintable(QString::number(cellDiff)));
    }

    void continuidadCurva()
    {
        double totalDiff, cellDiff;
        continuity(curveAt, 60, totalDiff, cellDiff);
        qInfo() << "curva 60 vs 180 muestras: total" << totalDiff * 100 << "% · por celda" << cellDiff * 100 << "%";
        QVERIFY2(totalDiff < 0.03, qPrintable(QString::number(totalDiff)));
        QVERIFY2(cellDiff < 0.20, qPrintable(QString::number(cellDiff)));
    }

    // Pasadas repetidas oscurecen cada vez menos: el diente se llena.
    void saturacion()
    {
        Paper paper(smallSheet());
        Pencil pencil(paper, Medium::hb());
        double antes = 0, aporteAnterior = 1e300;
        for (int pasada = 1; pasada <= 5; ++pasada) {
            draw(pencil, lineAt, 40);
            const double ahora = total(depositOf(paper));
            const double aporte = ahora - antes;
            qInfo() << "pasada" << pasada << "aporte" << aporte;
            QVERIFY(aporte > 0);
            QVERIFY2(aporte < aporteAnterior, qPrintable(QStringLiteral("pasada %1").arg(pasada)));
            aporteAnterior = aporte;
            antes = ahora;
        }
    }

    // Sin desplazamiento no hay deslizamiento: apoyar sin mover no deposita.
    void apoyarSinMoverNoDeposita()
    {
        Paper paper(smallSheet());
        Pencil pencil(paper, Medium::hb());
        pencil.beginStroke(lineAt(0.5));
        QVERIFY(pencil.strokeTo(lineAt(0.5)).empty());
        pencil.endStroke();
        QCOMPARE(paper.tileCount(), size_t(0));
    }

    // Regresión: un trazo fijo deja siempre el mismo papel (valor de referencia), igual
    // por la ruta AVX2 y por la escalar.
    void determinismoYRutas()
    {
        Paper scalar(smallSheet());
        Pencil ps(scalar, Medium::hb(), Contact::Path::Scalar);
        draw(ps, curveAt, 60);
        // Medido el 10 de octubre de 2026 (igual en Debug y Release). Si cambia el modelo a
        // propósito, se actualiza acá; si cambia sin querer, este test lo marca.
        QCOMPARE(scalar.hash(), uint64_t(0xfec6a7257dc7dc15ULL));
        if (Contact::avx2Available()) {
            Paper avx2(smallSheet());
            Pencil pv(avx2, Medium::hb(), Contact::Path::Avx2);
            draw(pv, curveAt, 60);
            QCOMPARE(avx2.hash(), scalar.hash());
        }
    }

    void velocidad()
    {
        // Garabato rápido: ~1000 mm/s a 133 muestras/s → ~178 celdas por muestra.
        Paper paper({.seed = 5});
        Pencil pencil(paper, Medium::hb());
        const double step = 1000.0 * kCellsPerMm / 133.0;
        pencil.beginStroke({1000, 1000, 0.6f, 0, 70});
        QElapsedTimer timer;
        timer.start();
        const int samples = 300;
        for (int i = 1; i <= samples; ++i) {
            const double a = i * 0.08;
            pencil.strokeTo({3500 + 2000 * std::sin(a * 0.37), 2400 + 1500 * std::sin(a * 0.53 + 1), 0.6f,
                             float(i % 360), 70});
            Q_UNUSED(step);
        }
        pencil.endStroke();
        const double ms = double(timer.nsecsElapsed()) / 1e6 / samples;
        qInfo() << "garabato:" << ms << "ms por muestra ·" << double(pencil.substeps()) / samples << "subpasos por muestra"
                << (pencil.path() == Contact::Path::Avx2 ? "(AVX2)" : "(escalar)");
    }
};

QTEST_GUILESS_MAIN(TestPencil)
#include "tst_pencil.moc"
