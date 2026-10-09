#include <drymedia/Contact.h>
#include <drymedia/Paper.h>
#include <drymedia/Tip.h>

#include <QRandomGenerator>
#include <QTest>

#include <cmath>
#include <numbers>

using namespace drymedia;

namespace {

// Huella: celdas de la punta con altura finita; devuelve el cociente largo/ancho de su
// caja (largo en x, ancho en y).
struct Box {
    int minX, maxX, minY, maxY;
    int width() const { return maxX - minX + 1; }
    int height() const { return maxY - minY + 1; }
};

Box footprintBox(const Tip& tip)
{
    const int w = tip.width(), h = tip.height();
    Box b{w, -1, h, -1};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (tip.heights()[y * w + x] != Tip::kNoContact) {
                b.minX = qMin(b.minX, x);
                b.maxX = qMax(b.maxX, x);
                b.minY = qMin(b.minY, y);
                b.maxY = qMax(b.maxY, y);
            }
    return b;
}

double footprintAspect(const Tip& tip)
{
    const Box b = footprintBox(tip);
    return double(b.width()) / double(b.height());
}

// Relieve promedio de las celdas en contacto.
double meanContactRelief(const Contact& contact, const Tip& tip, const Paper& paper, int x, int y)
{
    double sum = 0;
    int n = 0;
    const int w = tip.width();
    for (int r = 0; r < tip.height(); ++r)
        for (int c = 0; c < w; ++c)
            if (contact.penetration()[r * w + c] > 0) {
                sum += paper.reliefAt(x - tip.originX() + c, y - tip.originY() + r);
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

    // HU-59: vertical, el cono de siempre (mapa de 48, el mismo con cualquier azimut).
    void verticalSinCambios()
    {
        const Medium hb = Medium::hb();
        const Tip a = Tip::make(hb, 0, 90), b = Tip::make(hb, 137, 90);
        QCOMPARE(a.width(), Tip::kTipSize);
        QCOMPARE(a.height(), Tip::kTipSize);
        QCOMPARE(a.originX(), Tip::kTipCenter);
        QVERIFY(std::equal(a.heights(), a.heights() + a.cells(), b.heights()));
        const double radius = hb.leadDiameterMm * kCellsPerMm / 2.0;
        for (int y = 0; y < Tip::kTipSize; ++y)
            for (int x = 0; x < Tip::kTipSize; ++x) {
                const double rho = std::hypot(x + 0.5 - Tip::kTipCenter, y + 0.5 - Tip::kTipCenter);
                const uint16_t esperado = rho <= radius ? uint16_t(std::round(rho * hb.coneSlope)) : Tip::kNoContact;
                QCOMPARE(a.heights()[y * Tip::kTipSize + x], esperado);
            }
    }

    // La altitud de la tableta se estira hasta el lápiz acostado: 90° queda igual, la
    // máxima inclinación (30°) es el costado del cono (12° + 1°).
    void inclinacionDeLaTableta()
    {
        const Medium hb = Medium::hb();
        QCOMPARE(Tip::effectiveAltitude(hb, 90), 90.0);
        QVERIFY(qAbs(Tip::effectiveAltitude(hb, 30) - 13.0) < 1e-9);
        QVERIFY(qAbs(Tip::effectiveAltitude(hb, 20) - 13.0) < 1e-9);
        double previa = 91;
        for (int alt = 90; alt >= 30; alt -= 5) {
            const double e = Tip::effectiveAltitude(hb, alt);
            QVERIFY(e < previa);
            previa = e;
        }
    }

    // Inclinada, la huella empieza en el vértice (el centro del mapa) y crece hacia donde
    // se inclina el lápiz: casi toda queda de ese lado.
    void huellaAsimetricaHaciaElAzimut()
    {
        const Medium hb = Medium::hb();
        for (const float az : {0.0f, 90.0f, 180.0f, 270.0f}) {
            const Tip tip = Tip::make(hb, az, 50);
            const int w = tip.width(), ox = tip.originX(), oy = tip.originY();
            const double dx = std::cos(az * std::numbers::pi / 180), dy = std::sin(az * std::numbers::pi / 180);
            int adelante = 0, atras = 0;
            for (int y = 0; y < tip.height(); ++y)
                for (int x = 0; x < w; ++x)
                    if (tip.heights()[y * w + x] != Tip::kNoContact)
                        ((x + 0.5 - ox) * dx + (y + 0.5 - oy) * dy > 0 ? adelante : atras)++;
            QVERIFY2(adelante > 4 * atras, qPrintable(QStringLiteral("az %1: %2 / %3").arg(az).arg(adelante).arg(atras)));
            // Lo más bajo está junto al vértice (en la esquina de las celdas del centro; cerca
            // del vértice el cono es más fino que una celda y puede no cubrir esas cuatro).
            const uint16_t* h = tip.heights();
            const int lowest = int(std::min_element(h, h + tip.cells()) - h);
            QVERIFY2(std::hypot(lowest % w + 0.5 - ox, lowest / w + 0.5 - oy) < 4.0, qPrintable(QString::number(lowest)));
            // Mapa ajustado: no mucho más grande que la huella.
            const Box b = footprintBox(tip);
            QVERIFY2(tip.width() <= b.width() + 8 && tip.height() <= b.height() + 8,
                     qPrintable(QStringLiteral("%1x%2").arg(tip.width()).arg(tip.height())));
        }
    }

    // Cuanto más se acuesta, más larga la huella en la dirección del azimut, y no más
    // ancha que la mina.
    void inclinacionAlargaLaHuella()
    {
        const Medium hb = Medium::hb();
        const int ancho = footprintBox(Tip::make(hb, 0, 90)).height();
        int previo = 0;
        for (const float alt : {90.0f, 75.0f, 60.0f, 45.0f, 30.0f}) {
            const Box b = footprintBox(Tip::make(hb, 0, alt));
            QVERIFY2(b.width() > previo, qPrintable(QStringLiteral("%1: %2").arg(alt).arg(b.width())));
            QVERIFY(b.height() <= ancho + 1);
            previo = b.width();
        }
        // Acostada: el costado del cono de mina, de largo radio / tan 12° ≈ 48 celdas.
        QVERIFY2(previo > 45 && previo < 60, qPrintable(QString::number(previo)));
        // Azimut 90° (hacia abajo): se alarga en y.
        QVERIFY(footprintAspect(Tip::make(hb, 90, 35)) < 0.5);
    }

    // Sin saltos: 1° de diferencia cambia poco la huella.
    void continuidadPorGrado()
    {
        const Medium hb = Medium::hb();
        for (int alt = 30; alt < 90; ++alt) {
            const int a = Tip::make(hb, 20, float(alt)).cellsInside();
            const int b = Tip::make(hb, 20, float(alt + 1)).cellsInside();
            QVERIFY2(std::abs(a - b) <= std::max(a, b) / 10 + 2,
                     qPrintable(QStringLiteral("%1: %2 / %3").arg(alt).arg(a).arg(b)));
        }
        const int a = Tip::make(hb, 20, 40).cellsInside(), b = Tip::make(hb, 21, 40).cellsInside();
        QVERIFY(std::abs(a - b) <= a / 20 + 2);
    }

    // El criterio de la historia: de costado el peso se reparte en más celdas, la punta se
    // hunde menos y toca más las crestas que la punta vertical con la misma presión.
    void costadoTocaLasCrestas()
    {
        const Paper paper({.seed = 3, .widthMm = 50, .heightMm = 50});
        const Medium hb = Medium::hb();
        const Tip vertical = Tip::make(hb, 0, 90), costado = Tip::make(hb, 0, 30);
        Contact contact;
        int crestas = 0;
        const int casos = 40;
        for (int i = 0; i < casos; ++i) {
            const int x = 150 + i * 25, y = 300 + (i % 7) * 40;
            contact.find(paper, vertical, hb, x, y, 0.5f);
            const double reliefVertical = meanContactRelief(contact, vertical, paper, x, y);
            const int celdasVertical = contact.cellsInContact();
            contact.find(paper, costado, hb, x, y, 0.5f);
            const double reliefCostado = meanContactRelief(contact, costado, paper, x, y);
            QVERIFY(contact.cellsInContact() > celdasVertical); // más área
            crestas += reliefCostado > reliefVertical ? 1 : 0;
        }
        QVERIFY2(crestas >= 34, qPrintable(QString::number(crestas)));
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
            const double reliefPoca = meanContactRelief(contact, tip, paper, x, y);
            contact.find(paper, tip, medium, x, y, 0.9f);
            const int muchas = contact.cellsInContact();
            const double reliefMucha = meanContactRelief(contact, tip, paper, x, y);
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
        // Depósito y bruñido al azar en algunos tiles, para que la superficie no sea solo relieve.
        for (int t = 0; t < 20; ++t) {
            const int tx = int(rng.bounded(paper.tilesX())), ty = int(rng.bounded(paper.tilesY()));
            uint16_t* tile = paper.depositTile(tx, ty);
            for (int i = 0; i < kTileStride; ++i)
                tile[i] = uint16_t(rng.bounded(65536));
            paper.markTilePlanes(tx, ty, 0x0E);
        }
        Contact scalar(Contact::Path::Scalar), avx2(Contact::Path::Avx2);
        QCOMPARE(avx2.path(), Contact::Path::Avx2);
        const Medium hb = Medium::hb();
        for (int i = 0; i < 500; ++i) {
            // También posiciones en el borde y fuera de la hoja.
            const int x = int(rng.bounded(paper.width() + 40)) - 20, y = int(rng.bounded(paper.height() + 40)) - 20;
            // Incluye el costado (por debajo de 30°): mapas más grandes que 48.
            const Tip tip = Tip::make(hb, float(rng.bounded(360.0)), float(20 + rng.bounded(70.0)));
            const float p = float(rng.bounded(1.0));
            const uint16_t ds = scalar.find(paper, tip, hb, x, y, p);
            const uint16_t dv = avx2.find(paper, tip, hb, x, y, p);
            QCOMPARE(dv, ds);
            QVERIFY(std::equal(scalar.penetration(), scalar.penetration() + tip.cells(), avx2.penetration()));
            // Depósito con k y techo al azar (HU-56): también idéntico.
            const uint16_t k = uint16_t(rng.bounded(65536)), ceiling = uint16_t(rng.bounded(65536));
            scalar.applyDeposit(k, ceiling);
            avx2.applyDeposit(k, ceiling);
            QVERIFY(std::equal(scalar.deposit(), scalar.deposit() + tip.cells(), avx2.deposit()));
            // Goma (HU-58): también idéntica.
            scalar.applyErase(k);
            avx2.applyErase(k);
            QVERIFY(std::equal(scalar.deposit(), scalar.deposit() + tip.cells(), avx2.deposit()));
            // Bruñido (HU-60): depósito emparejado y bruñido, idénticos.
            const uint16_t kb = uint16_t(rng.bounded(65536));
            scalar.applyBurnish(kb);
            avx2.applyBurnish(kb);
            QVERIFY(std::equal(scalar.deposit(), scalar.deposit() + tip.cells(), avx2.deposit()));
            QVERIFY(std::equal(scalar.burnish(), scalar.burnish() + tip.cells(), avx2.burnish()));
            // Deformación y daño de fibra (HU-61).
            const uint16_t rate = uint16_t(rng.bounded(65536));
            scalar.applyDeform(rate);
            avx2.applyDeform(rate);
            QVERIFY(std::equal(scalar.deform(), scalar.deform() + tip.cells(), avx2.deform()));
            scalar.applyDamage(rate);
            avx2.applyDamage(rate);
            QVERIFY(std::equal(scalar.damage(), scalar.damage() + tip.cells(), avx2.damage()));
        }
        // Y con la punta grande de la goma (otro tamaño de huella).
        const Medium goma = Medium::eraser(5.0, 40);
        const Tip tip = Tip::make(goma, 0, 90);
        for (int i = 0; i < 50; ++i) {
            const int x = int(rng.bounded(paper.width())), y = int(rng.bounded(paper.height()));
            const float p = float(rng.bounded(1.0));
            QCOMPARE(avx2.find(paper, tip, goma, x, y, p), scalar.find(paper, tip, goma, x, y, p));
            const uint16_t k = uint16_t(rng.bounded(65536));
            scalar.applyErase(k);
            avx2.applyErase(k);
            QVERIFY(std::equal(scalar.deposit(), scalar.deposit() + tip.cells(), avx2.deposit()));
        }
    }

    // La goma (HU-58): mapa más grande que el de las minas, múltiplo de 4, con la cara
    // plana (altura 0 en todo el centro) y redonda.
    void puntaDeGoma()
    {
        const Tip tip = Tip::make(Medium::eraser(5.0, 40), 37, 45);
        QCOMPARE(tip.width() % 4, 0);
        QCOMPARE(tip.height(), tip.width());
        QVERIFY(tip.width() >= int(5.0 * kCellsPerMm));
        const int c = tip.originX();
        const int flatRadius = int((2.5 - 0.3) * kCellsPerMm) - 1;
        QCOMPARE(tip.heights()[size_t(c) * size_t(tip.width()) + size_t(c)], uint16_t(0));
        QCOMPARE(tip.heights()[size_t(c) * size_t(tip.width()) + size_t(c + flatRadius)], uint16_t(0));
        QCOMPARE(tip.heights()[size_t(c + flatRadius) * size_t(tip.width()) + size_t(c)], uint16_t(0));
        QCOMPARE(tip.heights()[0], Tip::kNoContact); // esquina: fuera de la goma
        const double area = std::numbers::pi * std::pow(2.5 * kCellsPerMm, 2);
        QVERIFY(std::abs(tip.cellsInside() - area) < area * 0.02);
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
