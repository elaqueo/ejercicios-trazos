#include <drymedia/Contact.h>
#include <drymedia/Paper.h>
#include <drymedia/Tip.h>

#include <QRandomGenerator>
#include <QTest>

using namespace drymedia;

namespace {

// Huella: celdas de la punta con altura finita; devuelve el cociente largo/ancho de su
// caja (largo en x, ancho en y).
double footprintAspect(const Tip& tip)
{
    int minX = Tip::kTipSize, maxX = -1, minY = Tip::kTipSize, maxY = -1;
    for (int y = 0; y < Tip::kTipSize; ++y)
        for (int x = 0; x < Tip::kTipSize; ++x)
            if (tip.heights()[y * Tip::kTipSize + x] != Tip::kNoContact) {
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
    return double(maxX - minX + 1) / double(maxY - minY + 1);
}

// Relieve promedio de las celdas en contacto.
double meanContactRelief(const Contact& contact, const Paper& paper, int x, int y)
{
    double sum = 0;
    int n = 0;
    for (int r = 0; r < Tip::kTipSize; ++r)
        for (int c = 0; c < Tip::kTipSize; ++c)
            if (contact.penetration()[r * Tip::kTipSize + c] > 0) {
                sum += paper.reliefAt(x - Tip::kTipCenter + c, y - Tip::kTipCenter + r);
                ++n;
            }
    return n ? sum / n : 0;
}

} // namespace

class TestContact : public QObject {
    Q_OBJECT

private slots:
    void huellaVerticalRedonda()
    {
        const Tip tip = Tip::make(Medium::hb().withLeadDiameter(0.5), 0, 90);
        // Mina de 0,5 mm a 600 dpi: radio ≈ 5,9 celdas → ≈ 109 celdas.
        QVERIFY2(tip.cellsInside() > 90 && tip.cellsInside() < 130, qPrintable(QString::number(tip.cellsInside())));
        // La HB calibrada (0,87 mm): radio ≈ 10,3 celdas → ≈ 330 celdas.
        const int hb = Tip::make(Medium::hb(), 0, 90).cellsInside();
        QVERIFY2(hb > 300 && hb < 360, qPrintable(QString::number(hb)));
        QVERIFY(qAbs(footprintAspect(tip) - 1.0) < 0.1);
        QCOMPARE(tip.heights()[Tip::kTipCenter * Tip::kTipSize + Tip::kTipCenter] < 600, true); // centro, lo más bajo
    }

    void inclinacionAlargaLaHuella()
    {
        const Medium hb = Medium::hb();
        const double vertical = footprintAspect(Tip::make(hb, 0, 90));
        const double a60 = footprintAspect(Tip::make(hb, 0, 60));
        const double a35 = footprintAspect(Tip::make(hb, 0, 35));
        QVERIFY2(vertical < a60 && a60 < a35, qPrintable(QStringLiteral("%1 %2 %3").arg(vertical).arg(a60).arg(a35)));
        QVERIFY2(qAbs(a35 - 1.0 / std::sin(35 * 3.14159265 / 180)) < 0.25, qPrintable(QString::number(a35)));
        // Azimut 90° (hacia abajo): se alarga en y.
        QVERIFY(footprintAspect(Tip::make(hb, 90, 35)) < 0.8);
    }

    void presionCeroNoToca()
    {
        const Paper paper({.seed = 3, .widthMm = 30, .heightMm = 30});
        Contact contact;
        contact.find(paper, Tip::make(Medium::hb(), 0, 90), Medium::hb(), 300, 300, 0.0f);
        QCOMPARE(contact.cellsInContact(), 0);
    }

    // El criterio de la historia: con poca presión toca menos celdas y más altas (las
    // crestas); con más presión, más celdas y llega a los valles. Devuelve en cuántas de
    // las posiciones probadas las celdas de poca presión quedaron más altas.
    static int crestsAtLowPressure(const Medium& medium, int casos)
    {
        const Paper paper({.seed = 3, .widthMm = 50, .heightMm = 50});
        const Tip tip = Tip::make(medium, 0, 90);
        Contact contact;
        int crestas = 0;
        for (int i = 0; i < casos; ++i) {
            const int x = 100 + i * 25, y = 300 + (i % 7) * 40;
            contact.find(paper, tip, medium, x, y, 0.15f);
            const int pocas = contact.cellsInContact();
            const double reliefPoca = meanContactRelief(contact, paper, x, y);
            contact.find(paper, tip, medium, x, y, 0.9f);
            const int muchas = contact.cellsInContact();
            const double reliefMucha = meanContactRelief(contact, paper, x, y);
            if (pocas == 0 || muchas <= pocas)
                return -1; // con poca presión tiene que tocar, y con más, tocar más
            crestas += reliefPoca > reliefMucha ? 1 : 0;
        }
        return crestas;
    }

    // El modelo de contacto, con la punta fina de referencia (0,5 mm): casi siempre.
    void pocaPresionSoloCrestas()
    {
        const int crestas = crestsAtLowPressure(Medium::hb().withLeadDiameter(0.5), 40);
        QVERIFY2(crestas >= 36, qPrintable(QString::number(crestas)));
    }

    // La HB calibrada (0,87 mm) cubre más de una ondulación del papel (~8 celdas), así que
    // el cono pesa más que el relieve y la preferencia por las crestas es más débil (35 de
    // 40 al calibrar), como en una mina real más gruesa. Se pide una mayoría clara.
    void pocaPresionSoloCrestasConLaHb()
    {
        const int crestas = crestsAtLowPressure(Medium::hb(), 40);
        QVERIFY2(crestas >= 30, qPrintable(QString::number(crestas)));
    }

    // La fuerza lograda alcanza el objetivo, y la misma entrada da la misma salida.
    void laFuerzaAlcanzaElObjetivoYEsDeterminista()
    {
        const Paper paper({.seed = 3, .widthMm = 30, .heightMm = 30});
        const Medium hb = Medium::hb();
        const Tip tip = Tip::make(hb, 30, 70);
        Contact a, b;
        const uint16_t da = a.find(paper, tip, hb, 350, 350, 0.5f);
        QVERIFY(a.force() >= hb.forceScale / 2);
        QVERIFY(da < 65535);
        QCOMPARE(b.find(paper, tip, hb, 350, 350, 0.5f), da);
        QCOMPARE(b.force(), a.force());
    }

    // AVX2 y escalar dan exactamente lo mismo: profundidad y penetración celda por celda.
    void escalarIgualAvx2()
    {
        if (!Contact::avx2Available())
            QSKIP("La CPU no tiene AVX2");
        Paper paper({.seed = 11, .widthMm = 60, .heightMm = 60});
        QRandomGenerator rng(1234);
        // Depósito al azar en algunos tiles, para que la superficie no sea solo relieve.
        for (int t = 0; t < 20; ++t) {
            uint16_t* tile = paper.depositTile(int(rng.bounded(paper.tilesX())), int(rng.bounded(paper.tilesY())));
            for (int i = 0; i < kTileCells; ++i)
                tile[i] = uint16_t(rng.bounded(65536));
        }
        Contact scalar(Contact::Path::Scalar), avx2(Contact::Path::Avx2);
        QCOMPARE(avx2.path(), Contact::Path::Avx2);
        const Medium hb = Medium::hb();
        for (int i = 0; i < 500; ++i) {
            // También posiciones en el borde y fuera de la hoja.
            const int x = int(rng.bounded(paper.width() + 40)) - 20, y = int(rng.bounded(paper.height() + 40)) - 20;
            const Tip tip = Tip::make(hb, float(rng.bounded(360.0)), float(30 + rng.bounded(60.0)));
            const float p = float(rng.bounded(1.0));
            const uint16_t ds = scalar.find(paper, tip, hb, x, y, p);
            const uint16_t dv = avx2.find(paper, tip, hb, x, y, p);
            QCOMPARE(dv, ds);
            QVERIFY(std::equal(scalar.penetration(), scalar.penetration() + Tip::kTipCells, avx2.penetration()));
        }
    }

    void noModificaElPapel()
    {
        Paper paper({.seed = 3, .widthMm = 30, .heightMm = 30});
        const uint64_t antes = paper.hash();
        Contact contact;
        contact.find(paper, Tip::make(Medium::hb(), 0, 90), Medium::hb(), 300, 300, 1.0f);
        QCOMPARE(paper.hash(), antes);
        QCOMPARE(paper.tileCount(), size_t(0));
    }
};

QTEST_GUILESS_MAIN(TestContact)
#include "tst_contact.moc"
