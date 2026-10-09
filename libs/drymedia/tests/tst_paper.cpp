#include <drymedia/Paper.h>

#include <QElapsedTimer>
#include <QTest>

#include <set>

using drymedia::Paper;
using drymedia::PaperSpec;

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
};

QTEST_GUILESS_MAIN(TestPaper)
#include "tst_paper.moc"
