#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <QElapsedTimer>
#include <QFile>
#include <QTest>

#include <algorithm>
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

    // Techo de tono (HU-56): una mina dura no pasa su negro máximo por más que se insista,
    // pero se acerca a él.
    void techoNoSeSupera()
    {
        Paper paper(smallSheet());
        Medium hard = Medium::hb();
        hard.softness = 255;
        hard.ceiling = 20000;
        Pencil pencil(paper, hard);
        for (int pasada = 0; pasada < 10; ++pasada)
            draw(pencil, lineAt, 40);
        const std::vector<uint16_t> dep = depositOf(paper);
        const uint16_t maximo = *std::max_element(dep.begin(), dep.end());
        qInfo() << "máximo" << maximo;
        QVERIFY(maximo <= hard.ceiling);
        QVERIFY(maximo > hard.ceiling * 9 / 10);
    }

    // Pasar una mina dura sobre grafito más oscuro que su techo no lo aclara, y deja
    // igual cada celda que ya estaba por encima del techo.
    void techoNoAclara()
    {
        Paper paper(smallSheet());
        Medium soft = Medium::hb();
        soft.softness = 255;
        Pencil blanda(paper, soft);
        for (int pasada = 0; pasada < 3; ++pasada)
            draw(blanda, lineAt, 40);
        const std::vector<uint16_t> antes = depositOf(paper);
        Medium hard = Medium::hb();
        hard.ceiling = 15000;
        Pencil dura(paper, hard);
        draw(dura, lineAt, 40);
        const std::vector<uint16_t> despues = depositOf(paper);
        int arriba = 0;
        for (size_t i = 0; i < antes.size(); ++i) {
            QVERIFY(despues[i] >= antes[i]);
            if (antes[i] >= hard.ceiling) {
                ++arriba;
                QCOMPARE(despues[i], antes[i]);
            }
        }
        QVERIFY(arriba > 100); // la prueba tiene sentido: hay celdas por encima del techo
    }

    // Cruzar un trazo cargado no desvía el trazo nuevo: el depósito nuevo de cada fila de
    // un trazo vertical queda centrado en la línea, también donde cruza un trazo 6B
    // saturado (reporte del 10 de octubre de 2026, no reproducido: el desvío medido es de
    // ~3 celdas, 0,13 mm, en el cruce y lejos de él).
    void cruzarNoDesvia()
    {
        Paper paper(smallSheet());
        Medium soft = Medium::hb().withLeadDiameter(1.45);
        soft.softness = 70;
        Pencil pencil(paper, soft);
        const auto horizontal = [](double t) { return PencilSample{200 + t * 1400, 700, 1.0f, 0.0f, 55.0f}; };
        for (int pasada = 0; pasada < 12; ++pasada)
            draw(pencil, horizontal, 60);
        const std::vector<uint16_t> antes = depositOf(paper);
        const double x = 900.3;
        const auto vertical = [x](double t) { return PencilSample{x, 400 + t * 600, 0.5f, 45.0f, 40.0f}; };
        draw(pencil, vertical, 60);
        const std::vector<uint16_t> despues = depositOf(paper);
        double peorLejos = 0, peorCruce = 0;
        for (int y = 450; y < 950; y += 2) {
            double suma = 0, momento = 0;
            for (int cx = 840; cx < 960; ++cx) {
                const size_t i = size_t(y) * size_t(paper.width()) + size_t(cx);
                const double d = double(despues[i]) - double(antes[i]);
                suma += d;
                momento += d * cx;
            }
            if (suma <= 0)
                continue;
            double& peor = std::abs(y - 700) < 60 ? peorCruce : peorLejos;
            peor = std::max(peor, std::abs(momento / suma - x));
        }
        qInfo() << "desvío máximo en celdas: lejos" << peorLejos << "· en el cruce" << peorCruce;
        QVERIFY2(peorCruce < 5.0, qPrintable(QString::number(peorCruce))); // 0,2 mm
    }

    // Diagnóstico: re-simula una grabación de Cartuchera (--grabar) y guarda el depósito
    // como PGM (reducido 4:1, el más oscuro de cada bloque). Solo corre con DRYMEDIA_REPLAY.
    void reproducirGrabacion()
    {
        const QString csv = qEnvironmentVariable("DRYMEDIA_REPLAY");
        if (csv.isEmpty())
            QSKIP("sin DRYMEDIA_REPLAY");
        QFile in(csv);
        QVERIFY(in.open(QIODevice::ReadOnly | QIODevice::Text));
        in.readLine();
        Paper paper;
        Pencil pencil(paper, Medium::hb());
        QString lead;
        while (!in.atEnd()) {
            const QList<QByteArray> f = in.readLine().trimmed().split(',');
            if (f.size() < 13)
                continue;
            const QString thisLead = QString::fromLatin1(f[10] + ',' + f[11] + ',' + f[12]);
            if (thisLead != lead) {
                if (pencil.inStroke())
                    pencil.endStroke();
                lead = thisLead;
                Medium m = Medium::hb().withLeadDiameter(f[11].toInt() / 100.0);
                m.softness = uint16_t(f[10].toInt());
                m.ceiling = uint16_t(f[12].toInt());
                pencil.setMedium(m);
            }
            const PencilSample sample{f[3].toDouble(), f[4].toDouble(), f[5].toFloat(), f[6].toFloat(), f[7].toFloat()};
            if (f[8] == "1" && f[9] == "0") {
                if (!pencil.inStroke())
                    pencil.beginStroke(sample);
                else
                    pencil.strokeTo(sample);
            } else if (pencil.inStroke()) {
                pencil.endStroke();
            }
        }
        if (pencil.inStroke())
            pencil.endStroke();
        const std::vector<uint16_t> dep = depositOf(paper);
        const int w = paper.width() / 4, h = paper.height() / 4;
        QByteArray pgm = QStringLiteral("P5 %1 %2 255\n").arg(w).arg(h).toLatin1();
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                uint16_t v = 0;
                for (int dy = 0; dy < 4; ++dy)
                    for (int dx = 0; dx < 4; ++dx)
                        v = std::max(v, dep[size_t(y * 4 + dy) * size_t(paper.width()) + size_t(x * 4 + dx)]);
                pgm.append(char(255 - v / 257));
            }
        QFile out(csv + QStringLiteral(".pgm"));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(pgm);
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
        if (Contact::avx2Available()) {
            Paper avx2(smallSheet());
            Pencil pv(avx2, Medium::hb(), Contact::Path::Avx2);
            draw(pv, curveAt, 60);
            QCOMPARE(avx2.hash(), scalar.hash());
            // Con techo (HU-56), también idénticas.
            Medium hard = Medium::hb();
            hard.ceiling = 12000;
            Paper hs(smallSheet()), hv(smallSheet());
            Pencil phs(hs, hard, Contact::Path::Scalar), phv(hv, hard, Contact::Path::Avx2);
            draw(phs, curveAt, 60);
            draw(phv, curveAt, 60);
            QCOMPARE(hv.hash(), hs.hash());
            QVERIFY(hs.hash() != scalar.hash());
        }
        // Medido el 10 de octubre de 2026 (igual en Debug y Release). Si cambia el modelo a
        // propósito, se actualiza acá; si cambia sin querer, este test lo marca.
        qInfo() << "hash" << Qt::hex << scalar.hash();
        QCOMPARE(scalar.hash(), uint64_t(0x49030d56c130aadcULL));
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
