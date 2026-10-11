#include <drymedia/Paper.h>

#include <QElapsedTimer>
#include <QTest>

#include <algorithm>
#include <cmath>

#include <set>

using drymedia::Paper;
using drymedia::PaperSpec;
using drymedia::TileSet;
using drymedia::kBurnishPlane;
using drymedia::kTileCells;
using drymedia::kTileStride;

class TestPaper : public QObject {
    Q_OBJECT

private slots:
    // A4 apaisado recortado (297 × 203 mm) a 600 dpi.
    void dimensionesDeLaHoja()
    {
        const Paper paper;
        QCOMPARE(paper.width(), 7016);
        QCOMPARE(paper.height(), 4795);
        QCOMPARE(paper.tilesX(), 110);
        QCOMPARE(paper.tilesY(), 75);
    }

    void mismaSemillaMismoRelieve()
    {
        QElapsedTimer timer;
        timer.start();
        const Paper a({.seed = 1});
        qInfo() << "relieve A4 generado en" << timer.elapsed() << "ms";
        const Paper b({.seed = 1});
        const Paper c({.seed = 2});
        QCOMPARE(a.reliefHash(), b.reliefHash());
        QVERIFY(a.reliefHash() != c.reliefHash());
    }

    // El relieve no depende de cuántos hilos lo generan ni de la máquina: el valor de
    // esta celda está fijo. Si cambia la fórmula, este test lo marca a propósito.
    void relieveFijoPorFormula()
    {
        // Valores de referencia de la fórmula con semilla 1 (enteros: iguales en cualquier
        // CPU y con cualquier cantidad de hilos). Medidos el 10 de octubre de 2026.
        const Paper paper({.seed = 1, .widthMm = 20, .heightMm = 20});
        QCOMPARE(paper.reliefAt(0, 0), uint16_t(1848));
        QCOMPARE(paper.reliefAt(5, 7), uint16_t(1453));
        QCOMPARE(paper.reliefAt(100, 200), uint16_t(1468));
        QCOMPARE(paper.reliefHash(), uint64_t(0xca7260ca69a31f9dULL));
        // No depende del tamaño de la hoja.
        const Paper otra({.seed = 1, .widthMm = 40, .heightMm = 10});
        QCOMPARE(otra.reliefAt(5, 7), uint16_t(1453));
    }

    void relieveConGranoYOndulacion()
    {
        const Paper paper({.seed = 7, .widthMm = 50, .heightMm = 50});
        const int n = paper.width() * paper.height();
        qint64 sum = 0;
        int minV = 65535, maxV = 0, vecinosDistintos = 0;
        for (int i = 0; i < n; ++i) {
            const int v = paper.relief()[i];
            sum += v;
            minV = qMin(minV, v);
            maxV = qMax(maxV, v);
            if (i > 0 && v != paper.relief()[i - 1])
                ++vecinosDistintos;
        }
        const double mean = double(sum) / n;
        qInfo() << "relieve: min" << minV << "max" << maxV << "promedio" << mean;
        QVERIFY(minV >= 0 && maxV <= 4094);
        QVERIFY2(qAbs(mean - 2047) < 150, qPrintable(QString::number(mean)));
        QVERIFY(maxV - minV > 3000);               // rango amplio
        QVERIFY(vecinosDistintos > n * 9 / 10);    // grano fino: celdas vecinas distintas
    }

    void tilesSoloDondeSeToca()
    {
        Paper paper({.seed = 1, .widthMm = 50, .heightMm = 50});
        QCOMPARE(paper.tileCount(), size_t(0));
        QVERIFY(!paper.findDepositTile(3, 4));

        uint16_t* tile = paper.depositTile(3, 4);
        QVERIFY(tile);
        QCOMPARE(paper.depositTile(3, 4), tile); // el mismo
        QCOMPARE(paper.findDepositTile(3, 4), tile);
        QCOMPARE(paper.tileCount(), size_t(1));
        for (int i = 0; i < drymedia::kTileCells; ++i)
            QCOMPARE(tile[i], uint16_t(0));

        // Fuera de la hoja no se crea nada.
        QVERIFY(!paper.depositTile(-1, 0));
        QVERIFY(!paper.depositTile(paper.tilesX(), 0));
        QVERIFY(!paper.depositTile(0, paper.tilesY()));
        QCOMPARE(paper.tileCount(), size_t(1));
    }

    // Más tiles que un bloque del pool: todos distintos y en cero.
    void poolMasAllaDeUnBloque()
    {
        Paper paper({.seed = 1, .widthMm = 297, .heightMm = 203});
        std::set<uint16_t*> seen;
        for (int ty = 0; ty < 10; ++ty)
            for (int tx = 0; tx < 60; ++tx) {
                uint16_t* t = paper.depositTile(tx, ty);
                QVERIFY(t && t[0] == 0 && t[drymedia::kTileCells - 1] == 0);
                t[0] = 1; // si dos tiles se pisaran, el siguiente no estaría en cero
                seen.insert(t);
            }
        QCOMPARE(seen.size(), size_t(600));
        QCOMPARE(paper.tileCount(), size_t(600));
    }

    // Hoja nueva: sin depósito, mismo relieve, y los tiles se reusan en cero.
    void clearVuelveALaHojaNueva()
    {
        Paper paper({.seed = 1, .widthMm = 50, .heightMm = 50});
        uint16_t* tile = paper.depositTile(1, 1);
        tile[5] = 999;
        paper.clear();
        QCOMPARE(paper.tileCount(), size_t(0));
        QVERIFY(!paper.findDepositTile(1, 1));
        QCOMPARE(paper.hash(), paper.reliefHash());
        uint16_t* again = paper.depositTile(4, 4);
        QCOMPARE(again, tile); // reusado del pool
        QCOMPARE(again[5], uint16_t(0));
    }

    void elHashSigueAlDeposito()
    {
        Paper a({.seed = 1, .widthMm = 50, .heightMm = 50});
        Paper b({.seed = 1, .widthMm = 50, .heightMm = 50});
        QCOMPARE(a.hash(), b.hash());
        QCOMPARE(a.hash(), a.reliefHash()); // sin depósito

        a.depositTile(2, 2)[100] = 500;
        QVERIFY(a.hash() != b.hash());
        b.depositTile(2, 2)[100] = 500;
        QCOMPARE(a.hash(), b.hash());

        // El mismo valor en otro tile da otro hash.
        Paper c({.seed = 1, .widthMm = 50, .heightMm = 50});
        c.depositTile(2, 3)[100] = 500;
        QVERIFY(c.hash() != a.hash());
    }

    // HU-82: el papel cambia de conjunto de tiles (de hoja) sin copiar; cada conjunto
    // conserva lo suyo y el relieve es el mismo para todos.
    // HU-112: la resolución es un dato del papel. A 300 dpi la hoja tiene la mitad de celdas
    // por lado y la ondulación conserva su tamaño físico: en los nodos de su grilla (cada 0,34
    // mm) las crestas son las mismas que a 600.
    void resolucionDelPapel()
    {
        const Paper fino({.seed = 4, .widthMm = 40, .heightMm = 30});
        const Paper grueso({.seed = 4, .widthMm = 40, .heightMm = 30, .dpi = 300});
        QCOMPARE(fino.width(), int(std::lround(40 * 600 / 25.4)));
        QCOMPARE(grueso.width(), int(std::lround(40 * 300 / 25.4)));
        QCOMPARE(grueso.height(), int(std::lround(30 * 300 / 25.4)));
        QCOMPARE(grueso.cellsPerMm(), 300 / 25.4);
        for (int j = 0; j < 40; ++j)
            for (int i = 0; i < 50; ++i)
                QCOMPARE(grueso.crest()[size_t(4 * j) * size_t(grueso.width()) + size_t(4 * i)],
                         fino.crest()[size_t(8 * j) * size_t(fino.width()) + size_t(8 * i)]);
    }

    // HU-112: una hoja que no es la del papel guarda de cada tile solo los planos que usa;
    // al volver es la misma. La hoja de Mesa de animación (la tableta entera, 600 dpi desde
    // HU-97) con el 26 % de los tiles dibujado (lo que midió el spike de HU-81) ocupa menos de
    // 20 MB (con los 4 planos, unos 77).
    void hojaGuardadaEmpaquetada()
    {
        Paper paper({.seed = 2, .widthMm = 325.1, .heightMm = 203.2});
        const int tilesX = paper.tilesX(), total = paper.tilesX() * paper.tilesY();
        int touched = 0;
        for (int i = 0; i < total; ++i)
            if (i % 100 < 26) {
                paper.depositTile(i % tilesX, i / tilesX)[i % kTileCells] = uint16_t(1000 + i % 7);
                ++touched;
            }
        paper.depositTile(0, 0)[kBurnishPlane * kTileCells + 5] = 77; // uno bruñido: ese plano también se guarda
        paper.setTilePlanes(0, 0, 1 << kBurnishPlane);
        const uint64_t hoja = paper.hash();

        TileSet guardada = paper.newTileSet();
        paper.swapTiles(guardada);
        QCOMPARE(guardada.tileCount(), size_t(touched));
        qInfo("Hoja de %d tiles con %d dibujados: %.1f MB guardada (%.1f MB con 4 planos)", total, touched,
              guardada.bytes() / 1048576.0, double(touched) * kTileStride * 2 / 1048576.0);
        QVERIFY(guardada.bytes() <= size_t(20) * 1024 * 1024);
        QVERIFY(guardada.bytes() < size_t(touched) * kTileStride * 2 / 3);

        paper.swapTiles(guardada);
        QCOMPARE(paper.hash(), hoja);
        QCOMPARE(paper.tilePlanes(0, 0), uint8_t(1 << kBurnishPlane));
        QCOMPARE(paper.depositTile(0, 0)[kBurnishPlane * kTileCells + 5], uint16_t(77));
    }

    // HU-97: la profundidad del grano es un dato del papel. El diente (cresta − relieve) llega
    // a lo sumo a esa profundidad; la ondulación no cambia. Con la de siempre, el relieve es el
    // de antes (el hash de referencia de tst_pencil no cambió).
    void profundidadDelGrano()
    {
        const Paper normal({.seed = 3, .widthMm = 30, .heightMm = 20});
        const Paper liso({.seed = 3, .widthMm = 30, .heightMm = 20, .grain = 900});
        const auto deepest = [](const Paper& p) {
            int most = 0;
            for (size_t i = 0; i < size_t(p.width()) * size_t(p.height()); ++i)
                most = std::max(most, int(p.crest()[i]) - int(p.relief()[i]));
            return most;
        };
        QVERIFY(deepest(normal) > 2000);
        QVERIFY(deepest(liso) <= 900);
        QVERIFY(deepest(liso) > 850);
        // La ondulación es la misma: donde el grano es 0 en los dos, el relieve coincide.
        for (size_t i = 0; i < size_t(normal.width()) * size_t(normal.height()); ++i)
            QCOMPARE(int(normal.crest()[i]) - 2047, int(liso.crest()[i]) - 900);
    }

    void cambiarDeConjuntoDeTiles()
    {
        Paper paper({.seed = 1, .widthMm = 50, .heightMm = 50});
        paper.depositTile(2, 2)[100] = 500;
        paper.setTilePlanes(2, 2, 0b0010);
        const uint64_t hojaA = paper.hash();

        drymedia::TileSet otra = paper.newTileSet();
        QCOMPARE(otra.tileCount(), size_t(0));
        paper.swapTiles(otra); // entra la hoja en blanco; la A queda en `otra`
        QCOMPARE(otra.tileCount(), size_t(1));
        QCOMPARE(paper.tileCount(), size_t(0));
        QCOMPARE(paper.hash(), paper.reliefHash());
        QCOMPARE(paper.tilePlanes(2, 2), uint8_t(0));

        paper.depositTile(3, 1)[7] = 900;
        const uint64_t hojaB = paper.hash();
        QVERIFY(hojaB != hojaA);

        paper.swapTiles(otra); // vuelve la A, tal cual (desempaquetada en otro lugar, HU-112)
        QCOMPARE(paper.hash(), hojaA);
        QVERIFY(paper.findDepositTile(2, 2) != nullptr);
        QCOMPARE(paper.tilePlanes(2, 2), uint8_t(0b0010));
        paper.swapTiles(otra);
        QCOMPARE(paper.hash(), hojaB);

        // Duplicar (HU-87): la copia tiene lo mismo, en otros tiles.
        drymedia::TileSet copia = paper.copyTiles();
        QCOMPARE(copia.tileCount(), paper.tileCount());
        const uint64_t original = paper.hash();
        paper.swapTiles(copia);
        QCOMPARE(paper.hash(), original);
        paper.depositTile(3, 1)[7] = 1; // tocar la copia no toca la original
        paper.swapTiles(copia);
        QCOMPARE(paper.hash(), original);

        // Un conjunto vacío (sin crear con newTileSet) entra como hoja en blanco.
        drymedia::TileSet vacio;
        paper.swapTiles(vacio);
        QCOMPARE(paper.hash(), paper.reliefHash());
        QCOMPARE(vacio.tileCount(), size_t(1));
    }
};

QTEST_GUILESS_MAIN(TestPaper)
#include "tst_paper.moc"
