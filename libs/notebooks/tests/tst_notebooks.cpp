#include <notebooks/Library.h>
#include <notebooks/Notebook.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace notebooks;

namespace {

Notebook diarios()
{
    Notebook nb;
    nb.name = QStringLiteral("Bocetos diarios");
    nb.collection = QString();
    nb.cover = QColor(0xA9, 0x53, 0x3D);
    nb.paper = paperPreset(QStringLiteral("kraft"));
    nb.maxSheets = 3;
    nb.started = QDate(2026, 10, 1);
    nb.updated = QDate(2026, 10, 12);
    return nb;
}

void write(const QString& path, const QByteArray& data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

// Un borrador: la carpeta de la hoja con su toma; terminada: además la imagen.
void draft(const QString& dir, int index)
{
    QVERIFY(QDir().mkpath(sheetFolder(dir, index)));
    write(QDir(sheetFolder(dir, index)).filePath(QStringLiteral("toma.json")), "{}");
}

void finish(const QString& dir, int index)
{
    draft(dir, index);
    write(QDir(sheetFolder(dir, index)).filePath(QString::fromLatin1(kFinishedImage)), "png");
}

} // namespace

class TestNotebooks : public QObject {
    Q_OBJECT

private slots:
    void loQueSeGuardaSeVuelveALeer()
    {
        QTemporaryDir dir;
        const Notebook nb = diarios();
        QVERIFY(saveNotebook(dir.path(), nb));
        LoadError error = LoadError::Corrupt;
        const std::optional<Notebook> back = loadNotebook(dir.path(), &error);
        QVERIFY(back);
        QCOMPARE(error, LoadError::None);
        QCOMPARE(back->name, nb.name);
        QCOMPARE(back->collection, nb.collection);
        QCOMPARE(back->cover, nb.cover);
        QCOMPARE(back->paper.preset, QStringLiteral("kraft"));
        QCOMPARE(back->paper.color, nb.paper.color);
        QCOMPARE(back->paper.grain, nb.paper.grain);
        QCOMPARE(back->paper.texture, nb.paper.texture);
        QCOMPARE(back->maxSheets, 3);
        QCOMPARE(back->started, nb.started);
        QCOMPARE(back->updated, nb.updated);
    }

    // Los valores del papel son los guardados, no los del papel de partida.
    void elPapelGuardadoManda()
    {
        QTemporaryDir dir;
        Notebook nb = diarios();
        nb.paper.color = QColor(0x12, 0x34, 0x56);
        nb.paper.grain = 900;
        QVERIFY(saveNotebook(dir.path(), nb));
        const std::optional<Notebook> back = loadNotebook(dir.path());
        QCOMPARE(back->paper.color, QColor(0x12, 0x34, 0x56));
        QCOMPARE(back->paper.grain, 900);
    }

    void versionNuevaAusenteODaniado()
    {
        QTemporaryDir dir;
        LoadError error = LoadError::None;
        QVERIFY(!loadNotebook(dir.path(), &error));
        QCOMPARE(error, LoadError::Missing);

        write(QDir(dir.path()).filePath(QString::fromLatin1(kNotebookFile)), "{ no es json");
        QVERIFY(!loadNotebook(dir.path(), &error));
        QCOMPARE(error, LoadError::Corrupt);

        write(QDir(dir.path()).filePath(QString::fromLatin1(kNotebookFile)),
              QStringLiteral(R"({"version": %1, "name": "Del futuro"})").arg(kNotebookVersion + 1).toUtf8());
        QVERIFY(!loadNotebook(dir.path(), &error));
        QCOMPARE(error, LoadError::NewerVersion);
    }

    void estadosDeLaHoja()
    {
        QTemporaryDir dir;
        QCOMPARE(sheetState(dir.path(), 1), SheetState::Empty);
        draft(dir.path(), 1);
        QCOMPARE(sheetState(dir.path(), 1), SheetState::Draft);
        finish(dir.path(), 1);
        QCOMPARE(sheetState(dir.path(), 1), SheetState::Finished);
        QCOMPARE(QDir(sheetFolder(dir.path(), 12)).dirName(), QStringLiteral("hoja-0012"));
    }

    // Se llena en orden: la siguiente es el borrador o la primera vacía; ninguna si está lleno.
    void laSiguienteHoja()
    {
        QTemporaryDir dir;
        const Notebook nb = diarios(); // 3 hojas
        QCOMPARE(nextSheet(dir.path(), nb), std::optional<int>(1));
        finish(dir.path(), 1);
        QCOMPARE(nextSheet(dir.path(), nb), std::optional<int>(2));
        draft(dir.path(), 2);
        QCOMPARE(nextSheet(dir.path(), nb), std::optional<int>(2));
        finish(dir.path(), 2);
        finish(dir.path(), 3);
        QCOMPARE(finishedSheets(dir.path(), nb), 3);
        QVERIFY(!nextSheet(dir.path(), nb));
    }

    void nombresDeCarpeta()
    {
        QCOMPARE(folderName(QStringLiteral("Bocetos diarios")), QStringLiteral("bocetos-diarios"));
        QCOMPARE(folderName(QStringLiteral("Diseño de personajes")), QStringLiteral("diseno-de-personajes"));
        QCOMPARE(folderName(QStringLiteral("  Proyecto XXX: ¡fondos!  ")), QStringLiteral("proyecto-xxx-fondos"));
        QCOMPARE(folderName(QStringLiteral("¿?")), QStringLiteral("cuaderno"));
    }

    // Dos cuadernos con el mismo nombre no chocan; la biblioteca los lista a los dos y salta lo
    // que no es un cuaderno.
    void crearYListar()
    {
        QTemporaryDir root;
        const QString library = QDir(root.path()).filePath(QStringLiteral("Bocetos"));
        Notebook a = diarios();
        const QString first = createNotebook(library, a);
        const QString second = createNotebook(library, a);
        QCOMPARE(QDir(first).dirName(), QStringLiteral("bocetos-diarios"));
        QCOMPARE(QDir(second).dirName(), QStringLiteral("bocetos-diarios-2"));
        Notebook b = diarios();
        b.name = QStringLiteral("Diseño de personajes");
        b.collection = QStringLiteral("Proyecto xxx");
        QVERIFY(!createNotebook(library, b).isEmpty());
        QVERIFY(QDir(library).mkdir(QStringLiteral("otra-cosa")));

        const QList<Entry> all = listNotebooks(library);
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].notebook.name, QStringLiteral("Bocetos diarios"));
        QCOMPARE(all[2].notebook.collection, QStringLiteral("Proyecto xxx"));
        QCOMPARE(QDir(all[2].dir).dirName(), QStringLiteral("diseno-de-personajes"));
    }

    void papelesDePartida()
    {
        QStringList ids;
        for (const Paper& p : paperPresets())
            ids << p.preset;
        QCOMPARE(ids, QStringList({"crema", "blanco", "kraft", "tonadoTostado", "tonadoGris"}));
        QCOMPARE(paperPreset(QStringLiteral("no-existe")).preset, QStringLiteral("crema"));
        QVERIFY(defaultLibraryRoot().endsWith(QStringLiteral("Trazos/Bocetos")));
    }
};

QTEST_MAIN(TestNotebooks)
#include "tst_notebooks.moc"
