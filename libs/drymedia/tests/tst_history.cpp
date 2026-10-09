#include <drymedia/History.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <QTest>

#include <algorithm>

using namespace drymedia;

namespace {

PaperSpec sheet()
{
    return {.seed = 9, .widthMm = 80, .heightMm = 60};
}

// Un lápiz conectado a un historial, como lo usa la app.
struct Rig {
    Paper paper{sheet()};
    Pencil pencil{paper, Medium::hb()};
    History history;

    explicit Rig(int limit = 100)
        : history(limit)
    {
        pencil.setTileObserver(
            [this](int i, const uint16_t* before, uint8_t planes) { history.beforeTileWrite(i, before, planes); });
    }

    // Trazo horizontal en la fila y.
    void stroke(double y, double x0 = 200, double x1 = 1200, float pressure = 0.6f)
    {
        history.beginStroke();
        pencil.beginStroke({x0, y, pressure, 0, 80});
        for (int i = 1; i <= 30; ++i)
            pencil.strokeTo({x0 + (x1 - x0) * i / 30.0, y, pressure, 0, 80});
        pencil.endStroke();
        history.endStroke(paper);
    }
};

} // namespace

class TestHistory : public QObject {
    Q_OBJECT

private slots:
    // HU-61: deshacer restaura también la deformación (el surco de la punta seca).
    void deshacerRestauraLaDeformacion()
    {
        Rig rig;
        rig.stroke(300);
        const uint64_t antes = rig.paper.hash();
        rig.pencil.setMedium(Medium::stylus());
        rig.stroke(300, 200, 1200, 1.0f);
        const uint16_t* tile = rig.paper.findDepositTile(400 / kTileSize, 300 / kTileSize);
        const uint16_t* deform = tile + kDeformPlane * kTileCells;
        QVERIFY(std::any_of(deform, deform + kTileCells, [](uint16_t v) { return v > 0; }));
        rig.history.undo(rig.paper);
        QCOMPARE(rig.paper.hash(), antes);
        QVERIFY(std::all_of(deform, deform + kTileCells, [](uint16_t v) { return v == 0; }));
    }

    // HU-60: deshacer restaura también el bruñido (el tile guarda los dos planos).
    void deshacerRestauraElBrunido()
    {
        Rig rig;
        rig.stroke(300);
        const uint64_t antes = rig.paper.hash();
        const uint16_t* tile = rig.paper.findDepositTile(400 / kTileSize, 300 / kTileSize);
        QVERIFY(tile);
        const std::vector<uint16_t> copia(tile, tile + kTileStride);
        rig.stroke(300, 200, 1200, 1.0f); // apretando: bruñe
        const uint16_t* burn = tile + kTileCells;
        QVERIFY(!std::equal(burn, burn + kTileCells, copia.begin() + kTileCells));
        rig.history.undo(rig.paper);
        QCOMPARE(rig.paper.hash(), antes);
        QVERIFY(std::equal(tile, tile + kTileStride, copia.begin()));
    }

    void deshacerYRehacerVuelvenAlMismoPapel()
    {
        Rig rig;
        const uint64_t vacio = rig.paper.hash();
        rig.stroke(300);
        const uint64_t conA = rig.paper.hash();
        rig.stroke(320); // se cruza en parte con A: hay tiles compartidos
        const uint64_t conAB = rig.paper.hash();
        QVERIFY(vacio != conA && conA != conAB);

        QVERIFY(!rig.history.undo(rig.paper).empty());
        QCOMPARE(rig.paper.hash(), conA);
        rig.history.undo(rig.paper);
        QCOMPARE(rig.paper.hash(), vacio);
        QCOMPARE(rig.paper.tileCount(), size_t(0)); // los tiles creados se devolvieron
        QVERIFY(!rig.history.canUndo());

        rig.history.redo(rig.paper);
        QCOMPARE(rig.paper.hash(), conA);
        rig.history.redo(rig.paper);
        QCOMPARE(rig.paper.hash(), conAB);
        QVERIFY(!rig.history.canRedo());
    }

    void trazoNuevoDescartaLoRehacible()
    {
        Rig rig;
        rig.stroke(300);
        rig.stroke(600);
        rig.history.undo(rig.paper);
        QVERIFY(rig.history.canRedo());
        rig.stroke(900);
        QVERIFY(!rig.history.canRedo());
        QCOMPARE(rig.history.undoCount(), size_t(2));
    }

    // Un trazo que no toca nada (apoyar sin mover) no ocupa lugar ni descarta lo rehacible.
    void trazoSinEfecto()
    {
        Rig rig;
        rig.stroke(300);
        rig.history.undo(rig.paper);
        rig.history.beginStroke();
        rig.pencil.beginStroke({500, 500, 0.6f, 0, 80});
        rig.pencil.endStroke();
        rig.history.endStroke(rig.paper);
        QVERIFY(rig.history.canRedo());
        QCOMPARE(rig.history.undoCount(), size_t(0));
    }

    void limiteDeTrazos()
    {
        Rig rig(5); // mismo mecanismo que con 100, más rápido
        for (int i = 0; i < 6; ++i)
            rig.stroke(200 + i * 150);
        QCOMPARE(rig.history.undoCount(), size_t(5));
        for (int i = 0; i < 5; ++i)
            rig.history.undo(rig.paper);
        QVERIFY(!rig.history.canUndo());
        QVERIFY(rig.paper.tileCount() > 0); // el primer trazo ya no se puede deshacer
    }

    // Las copias se toman antes de escribir: deshacer devuelve también el depósito que
    // había en un tile ya pintado por otro trazo.
    void tilesCompartidosConservanLoAnterior()
    {
        Rig rig;
        rig.stroke(300);
        const size_t tiles = rig.paper.tileCount();
        const uint64_t conA = rig.paper.hash();
        rig.stroke(300); // segunda pasada exactamente encima: mismos tiles
        QCOMPARE(rig.paper.tileCount(), tiles);
        rig.history.undo(rig.paper);
        QCOMPARE(rig.paper.hash(), conA);
        QCOMPARE(rig.paper.tileCount(), tiles);
    }
};

QTEST_GUILESS_MAIN(TestHistory)
#include "tst_history.moc"
