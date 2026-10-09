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

// Depósito medio (0..1) en el rectángulo [x0, x1) × [y0, y1).
double meanDeposit(const Paper& paper, int x0, int x1, int y0, int y1)
{
    const std::vector<uint16_t> d = depositOf(paper);
    double sum = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            sum += d[size_t(y) * size_t(paper.width()) + size_t(x)];
    return sum / (double(x1 - x0) * double(y1 - y0) * 65535.0);
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
    // un trazo vertical queda donde queda lejos del cruce, también donde cruza un trazo 6B
    // saturado (reporte del 10 de octubre de 2026, no reproducido). Desde HU-59 la huella
    // inclinada está corrida hacia el cuerpo del lápiz, así que la referencia es el centro
    // del mismo trazo lejos del cruce, no la posición del lápiz.
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
        // Lápiz poco inclinado, como en el reporte. De costado (HU-59) la huella mide ~70
        // celdas en diagonal y el centro por fila ya no mide un desvío: sobre el trazo
        // saturado casi no entra grafito nuevo y el depósito lo levanta un diente entero
        // (36 celdas con esta métrica; se revisa con el bruñido, HU-60).
        const auto vertical = [x](double t) { return PencilSample{x, 400 + t * 600, 0.5f, 45.0f, 75.0f}; };
        draw(pencil, vertical, 60);
        const std::vector<uint16_t> despues = depositOf(paper);
        std::vector<std::pair<int, double>> centros; // fila → centro del depósito nuevo
        for (int y = 500; y < 900; y += 2) {
            double suma = 0, momento = 0;
            for (int cx = 840; cx < 1000; ++cx) {
                const size_t i = size_t(y) * size_t(paper.width()) + size_t(cx);
                const double d = double(despues[i]) - double(antes[i]);
                suma += d;
                momento += d * cx;
            }
            if (suma > 0)
                centros.emplace_back(y, momento / suma);
        }
        double referencia = 0;
        int lejos = 0;
        for (const auto& [y, c] : centros)
            if (std::abs(y - 700) >= 100)
                referencia += c, ++lejos;
        QVERIFY(lejos > 50);
        referencia /= lejos;
        double peorLejos = 0, peorCruce = 0;
        for (const auto& [y, c] : centros) {
            double& peor = std::abs(y - 700) < 100 ? peorCruce : peorLejos;
            peor = std::max(peor, std::abs(c - referencia));
        }
        qInfo() << "centro lejos del cruce" << referencia - x << "celdas del lápiz · desvío máximo: lejos" << peorLejos
                << "· en el cruce" << peorCruce;
        // Con el grafito que llena el diente (HU-59): 6,6 celdas (antes 5,0); sobre el trazo
        // saturado casi no entra grafito nuevo y la métrica mide dónde queda lugar. De costado,
        // 18,6 (antes 36).
        QVERIFY2(peorCruce < 8.0, qPrintable(QString::number(peorCruce))); // 0,34 mm
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

    // Goma (HU-58): una pasada suave limpia sobre todo las crestas y deja casi todo el
    // trazo (lo que quita ∝ presión: el usuario la encontró demasiado fuerte apoyada
    // suave); pasadas fuertes dejan la hoja completamente limpia (decisión del usuario).
    void gomaSuaveDejaFantasma()
    {
        Paper paper(smallSheet());
        Medium soft = Medium::hb();
        soft.softness = 120;
        Pencil pencil(paper, soft);
        for (int pasada = 0; pasada < 3; ++pasada)
            draw(pencil, lineAt, 40);
        const double antes = total(depositOf(paper));
        Pencil goma(paper, Medium::eraser(5.0, 20));
        const auto suave = [](double t) { PencilSample s = lineAt(t); s.pressure = 0.15f; return s; };
        draw(goma, suave, 40);
        const double despues = total(depositOf(paper));
        qInfo() << "goma suave: queda" << despues / antes * 100 << "%";
        // Con el grafito dentro del diente (HU-59), una goma apenas apoyada toca también crestas
        // limpias: queda ~97 % (antes ~82 %, cuando el grafito sobresalía del papel).
        QVERIFY(despues < antes * 0.99);
        QVERIFY(despues > antes * 0.6);
    }

    void gomaFuerteLimpia()
    {
        Paper paper(smallSheet());
        Medium soft = Medium::hb();
        soft.softness = 120;
        Pencil pencil(paper, soft);
        for (int pasada = 0; pasada < 3; ++pasada)
            draw(pencil, lineAt, 40);
        Pencil goma(paper, Medium::eraser(5.0, 255));
        const auto fuerte = [](double t) { PencilSample s = lineAt(t); s.pressure = 1.0f; return s; };
        int pasadas = 0;
        while (total(depositOf(paper)) > 0 && pasadas < 30) {
            draw(goma, fuerte, 40);
            ++pasadas;
        }
        qInfo() << "goma fuerte: hoja limpia en" << pasadas << "pasadas";
        QCOMPARE(total(depositOf(paper)), 0.0);
    }

    // Borrar donde no hay grafito no crea tiles (ni memoria ni entradas en el historial).
    void gomaEnBlancoNoCreaTiles()
    {
        Paper paper(smallSheet());
        Pencil goma(paper, Medium::eraser(5.0, 40));
        draw(goma, lineAt, 40);
        QCOMPARE(paper.tileCount(), size_t(0));
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
        // Medido el 10 de octubre de 2026 (igual en Debug y Release); cambió a propósito con el
        // cono inclinado y el grafito que llena el diente (HU-59). Si cambia el modelo a propósito, se actualiza acá; si cambia
        // sin querer, este test lo marca.
        qInfo() << "hash" << Qt::hex << scalar.hash();
        QCOMPARE(scalar.hash(), uint64_t(0xaacc597fc5c52499ULL));
    }

    // Repasar de costado sigue oscureciendo (reporte del 9 de octubre de 2026): el grafito
    // llena el diente hasta las crestas de la zona y no más. Antes una celda saturada subía
    // un diente entero, la mina acostada quedaba apoyada en esas celdas y las pasadas
    // siguientes no agregaban nada.
    void repasarDeCostadoOscurece()
    {
        Paper paper(smallSheet());
        Medium m = Medium::hb().withLeadDiameter(1.45);
        m.softness = 70;
        Pencil pencil(paper, m);
        double tono[11] = {};
        for (int pasada = 1; pasada <= 10; ++pasada) {
            const double jx = (pasada % 3 - 1) * 3.0;
            draw(pencil, [jx](double t) { return PencilSample{900 + jx, 300 + t * 600, 0.6f, 90.0f, 32.0f}; }, 120);
            tono[pasada] = meanDeposit(paper, 885, 925, 450, 750);
        }
        qInfo() << "tono de la 6B de costado: pasada 1" << tono[1] << "· 3" << tono[3] << "· 10" << tono[10];
        // Con el modelo viejo: 0,088 · 0,091 · 0,092 (clavado); el canal de abajo quedaba en 0.
        QVERIFY2(tono[10] > tono[3] * 1.05 && tono[10] > 0.2, qPrintable(QStringLiteral("%1 %2").arg(tono[3]).arg(tono[10])));
    }

    // El caso del reporte: dos líneas oscuras y, entre ellas, un canal que la mina de costado
    // no podía pintar porque quedaba apoyada sobre las dos líneas.
    void costadoPintaEntreDosLineas()
    {
        Paper paper(smallSheet());
        Medium m = Medium::hb().withLeadDiameter(1.45);
        m.softness = 70;
        Pencil pencil(paper, m);
        for (int pasada = 0; pasada < 8; ++pasada)
            for (const double x : {880.0, 925.0})
                draw(pencil, [x](double t) { return PencilSample{x, 300 + t * 600, 1.0f, 0.0f, 88.0f}; }, 120);
        const double antes = meanDeposit(paper, 897, 909, 450, 750);
        for (int pasada = 0; pasada < 6; ++pasada)
            draw(pencil, [](double t) { return PencilSample{862, 300 + t * 600, 0.6f, 0.0f, 32.0f}; }, 120);
        const double canal = meanDeposit(paper, 897, 909, 450, 750), lineas = meanDeposit(paper, 876, 884, 450, 750);
        qInfo() << "canal antes" << antes << "· después" << canal << "· líneas" << lineas;
        QVERIFY2(canal > 0.3 * lineas, qPrintable(QStringLiteral("%1 / %2").arg(canal).arg(lineas)));
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

        // La goma de 5 mm con el mismo garabato, sobre lo que dejó el lápiz.
        Pencil goma(paper, Medium::eraser(5.0, 40));
        goma.beginStroke({1000, 1000, 0.6f, 0, 70});
        timer.restart();
        for (int i = 1; i <= samples; ++i) {
            const double a = i * 0.08;
            goma.strokeTo({3500 + 2000 * std::sin(a * 0.37), 2400 + 1500 * std::sin(a * 0.53 + 1), 0.6f, 0, 90});
        }
        goma.endStroke();
        qInfo() << "goma:" << double(timer.nsecsElapsed()) / 1e6 / samples << "ms por muestra ·"
                << double(goma.substeps()) / samples << "subpasos por muestra";
    }
};

QTEST_GUILESS_MAIN(TestPencil)
#include "tst_pencil.moc"
