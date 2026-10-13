#include "BocetosCore.h"
#include "SheetCache.h"

#include <appkit/Theme.h>

#include <notebooks/Library.h>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace bocetos;

namespace {

notebooks::Entry entry(const QString& dir, const QString& name, const QString& collection, const QDate& updated, int sheets = 10)
{
    notebooks::Notebook nb;
    nb.name = name;
    nb.collection = collection;
    nb.maxSheets = sheets;
    nb.started = QDate(2026, 9, 1);
    nb.updated = updated;
    return {dir, nb};
}

void touch(const QString& path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
}

// n libros en un estante.
QList<Shelf> shelf(int n)
{
    Shelf s;
    for (int i = 0; i < n; ++i)
        s.books.append(Book{});
    return {s};
}

} // namespace

class TestBocetos : public QObject {
    Q_OBJECT

private slots:
    void laBiblioteca()
    {
        QTemporaryDir dir;
        QCOMPARE(QDir(bocetos::libraryRoot({"bocetos.exe", "--biblioteca", dir.path()})), QDir(dir.path()));
        QCOMPARE(bocetos::libraryRoot({"bocetos.exe"}), notebooks::defaultLibraryRoot());
        QCOMPARE(bocetos::libraryRoot({"bocetos.exe", "--biblioteca"}), notebooks::defaultLibraryRoot());
    }

    void encuentraCartuchera()
    {
        QTemporaryDir dir;
        QVERIFY(bocetos::findCartuchera(dir.path()).isEmpty());
        QFile exe(QDir(dir.path()).filePath(QStringLiteral("cartuchera.exe")));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();
        QCOMPARE(QFileInfo(bocetos::findCartuchera(dir.path())), QFileInfo(exe.fileName()));
        // Una carpeta con ese nombre no es Cartuchera.
        QTemporaryDir other;
        QVERIFY(QDir(other.path()).mkdir(QStringLiteral("cartuchera.exe")));
        QVERIFY(bocetos::findCartuchera(other.path()).isEmpty());
    }

    // El general primero y las colecciones por nombre; adentro, la última actualización primero.
    void estantes()
    {
        const QList<Shelf> shelves = shelvesOf({entry("a", "Gestos", "", QDate(2026, 10, 2)),
                                                entry("b", "Personajes", "Proyecto xxx", QDate(2026, 10, 10)),
                                                entry("c", "Diarios", "", QDate(2026, 10, 12)),
                                                entry("d", "Animales", "Estudios", QDate(2026, 10, 1))});
        QCOMPARE(shelves.size(), 3);
        QCOMPARE(shelfTitle(shelves[0]), QStringLiteral("General"));
        QCOMPARE(shelves[0].books[0].notebook.name, QStringLiteral("Diarios"));
        QCOMPARE(shelves[0].books[1].notebook.name, QStringLiteral("Gestos"));
        QCOMPARE(shelfTitle(shelves[1]), QStringLiteral("Estudios"));
        QCOMPARE(shelfTitle(shelves[2]), QStringLiteral("Proyecto xxx"));
    }

    // Lo que dice la tapa: hojas usadas, borrador, lleno; y las fechas.
    void laTapa()
    {
        QTemporaryDir dir;
        notebooks::Notebook nb;
        nb.name = QStringLiteral("Diarios");
        nb.maxSheets = 2;
        nb.started = QDate(2026, 10, 1);
        nb.updated = QDate(2026, 10, 12);
        Book book = bookOf({dir.path(), nb});
        QCOMPARE(book.used, 0);
        QVERIFY(!book.draft && !book.full);
        QCOMPARE(bookDates(book, QDate(2026, 10, 13)), QStringLiteral("desde 1 oct"));

        QVERIFY(QDir().mkpath(notebooks::sheetFolder(dir.path(), 1)));
        touch(QDir(notebooks::sheetFolder(dir.path(), 1)).filePath(QString::fromLatin1(notebooks::kFinishedImage)));
        QVERIFY(QDir().mkpath(notebooks::sheetFolder(dir.path(), 2)));
        book = bookOf({dir.path(), nb});
        QCOMPARE(book.used, 1);
        QVERIFY(book.draft && !book.full);
        QCOMPARE(bookDates(book, QDate(2026, 10, 13)), QStringLiteral("desde 1 oct · última 12 oct"));
        QCOMPARE(bookDates(book, QDate(2027, 1, 2)), QStringLiteral("desde 1 oct 2026 · última 12 oct 2026"));

        touch(QDir(notebooks::sheetFolder(dir.path(), 2)).filePath(QString::fromLatin1(notebooks::kFinishedImage)));
        book = bookOf({dir.path(), nb});
        QVERIFY(book.full && !book.draft);
        QCOMPARE(book.used, 2);
    }

    // En 1000 px entran 3 por fila: 5 libros son dos filas; el estante siguiente va debajo.
    void acomodo()
    {
        QList<Shelf> shelves = shelf(5);
        shelves += shelf(2);
        const Layout layout = layoutLibrary(shelves, 1000);
        QCOMPARE(layout.items.size(), 7);
        QCOMPARE(layout.items[2].column, 2);
        QCOMPARE(layout.items[3].row, 1);
        QCOMPARE(layout.items[3].column, 0);
        QCOMPARE(layout.items[3].cover.left(), kMargin);
        QVERIFY(layout.items[3].cover.top() > layout.items[0].cover.bottom());
        QCOMPARE(layout.items[5].shelf, 1);
        QCOMPARE(layout.items[5].row, 2);
        QVERIFY(layout.marks[1].labelY > layout.marks[0].lineY);
        QVERIFY(layout.items[5].cover.top() > layout.marks[1].labelY);
        // Angosto: uno por fila.
        QCOMPARE(layoutLibrary(shelf(3), 100).items[2].row, 2);
    }

    void navegacion()
    {
        QList<Shelf> shelves = shelf(5); // filas 0 (3 libros) y 1 (2)
        shelves += shelf(1);             // fila 2
        const Layout layout = layoutLibrary(shelves, 1000);
        QCOMPARE(moveSelection(layout, 0, Move::Left), 0);
        QCOMPARE(moveSelection(layout, 2, Move::Right), 3); // pasa de fila
        QCOMPARE(moveSelection(layout, 4, Move::Right), 5); // y de estante
        QCOMPARE(moveSelection(layout, 5, Move::Right), 5);
        QCOMPARE(moveSelection(layout, 2, Move::Down), 4);  // columna 2 → la más cercana, la 1
        QCOMPARE(moveSelection(layout, 1, Move::Down), 4);
        QCOMPARE(moveSelection(layout, 4, Move::Down), 5);  // al estante de abajo
        QCOMPARE(moveSelection(layout, 5, Move::Up), 3);
        QCOMPARE(moveSelection(layout, 0, Move::Up), 0);
        QCOMPARE(moveSelection(layout, 5, Move::Down), 5);
        QCOMPARE(moveSelection(Layout{}, 0, Move::Right), -1);

        const QRect cover = layout.items[4].cover;
        QCOMPARE(slotAt(layout, cover.center()), 4);
        QCOMPARE(slotAt(layout, QPoint(cover.center().x(), cover.bottom() + kCanto)), 4); // el canto
        QCOMPARE(slotAt(layout, QPoint(5, 5)), -1);
    }

    void nombreSiguiente()
    {
        QCOMPARE(continuedName(QStringLiteral("Bocetos diarios")), QStringLiteral("Bocetos diarios 2"));
        QCOMPARE(continuedName(QStringLiteral("Bocetos diarios 2")), QStringLiteral("Bocetos diarios 3"));
        QCOMPARE(continuedName(QStringLiteral("Gestos 9 ")), QStringLiteral("Gestos 10"));
        QCOMPARE(continuedName(QStringLiteral("2026")), QStringLiteral("2026 2"));
    }

    void colecciones()
    {
        const QList<Shelf> shelves = shelvesOf({entry("a", "A", "", QDate(2026, 10, 2)), entry("b", "B", "Proyecto xxx", QDate(2026, 10, 2)),
                                                entry("c", "C", "Estudios", QDate(2026, 10, 2))});
        QCOMPARE(collectionsOf(shelves), QStringList({"Estudios", "Proyecto xxx"}));
    }

    // Cuaderno nuevo: lo de siempre, o seguir en uno nuevo si el elegido está lleno.
    void loQuePropone()
    {
        const Draft empty = draftFor(nullptr);
        QVERIFY(empty.name.isEmpty() && empty.collection.isEmpty());
        QCOMPARE(empty.cover, appkit::theme::kTapas[0]);
        QCOMPARE(empty.paper.preset, QStringLiteral("crema"));
        QCOMPARE(empty.maxSheets, 40);

        Book book;
        book.notebook.name = QStringLiteral("Gestos");
        book.notebook.collection = QStringLiteral("Estudios");
        book.notebook.cover = appkit::theme::kTapas[3];
        book.notebook.paper = notebooks::paperPreset(QStringLiteral("kraft"));
        book.notebook.maxSheets = 24;
        const Draft same = draftFor(&book);
        QVERIFY(same.name.isEmpty());
        QCOMPARE(same.collection, QStringLiteral("Estudios"));
        QCOMPARE(same.paper.preset, QStringLiteral("crema"));

        book.full = true;
        const Draft next = draftFor(&book);
        QCOMPARE(next.name, QStringLiteral("Gestos 2"));
        QCOMPARE(next.collection, QStringLiteral("Estudios"));
        QCOMPARE(next.cover, appkit::theme::kTapas[3]);
        QCOMPARE(next.paper.preset, QStringLiteral("kraft"));
        QCOMPARE(next.maxSheets, 24);
    }

    void cuadernoDelBorrador()
    {
        Draft draft;
        draft.name = QStringLiteral("  Gestos ");
        draft.collection = QStringLiteral(" Estudios ");
        draft.cover = appkit::theme::kTapas[1];
        draft.paper = notebooks::paperPreset(QStringLiteral("tonadoGris"));
        draft.maxSheets = 999;
        const notebooks::Notebook nb = notebookFrom(draft, QDate(2026, 10, 13));
        QCOMPARE(nb.name, QStringLiteral("Gestos"));
        QCOMPARE(nb.collection, QStringLiteral("Estudios"));
        QCOMPARE(nb.maxSheets, kMaxSheets);
        QCOMPARE(nb.started, QDate(2026, 10, 13));
        QCOMPARE(nb.updated, QDate(2026, 10, 13));
        QCOMPARE(nb.paper.preset, QStringLiteral("tonadoGris"));
    }
    // HU-137: las páginas del cuaderno abierto: las terminadas y la siguiente como aviso.
    void paginas()
    {
        QTemporaryDir tmp;
        notebooks::Notebook nb;
        nb.name = QStringLiteral("Gestos");
        nb.maxSheets = 4;
        const QString dir = tmp.path();
        // Sin hojas: solo la 1 en blanco, y abre ahí.
        QList<Page> pages = pagesOf(dir, nb);
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages[0].sheet, 1);
        QCOMPARE(pages[0].state, notebooks::SheetState::Empty);
        QCOMPARE(openingPage(pages), 0);
        QCOMPARE(usageText(pages, 4), QStringLiteral("0 de 4 usadas · sigue la 1"));
        // Dos terminadas y un borrador: abre en la segunda terminada.
        for (int i = 1; i <= 3; ++i)
            QVERIFY(QDir().mkpath(notebooks::sheetFolder(dir, i)));
        touch(notebooks::sheetFolder(dir, 1) + QStringLiteral("/terminada.png"));
        touch(notebooks::sheetFolder(dir, 2) + QStringLiteral("/terminada.png"));
        pages = pagesOf(dir, nb);
        QCOMPARE(pages.size(), 3);
        QCOMPARE(pages[1].state, notebooks::SheetState::Finished);
        QVERIFY(pages[1].image.endsWith(QStringLiteral("hoja-0002/terminada.png")));
        QCOMPARE(pages[1].finished, QDate::currentDate());
        QCOMPARE(pages[2].state, notebooks::SheetState::Draft);
        QCOMPARE(openingPage(pages), 1);
        QCOMPARE(usageText(pages, 4), QStringLiteral("2 de 4 usadas · borrador en la 3"));
        // Lleno: sin aviso.
        QVERIFY(QDir().mkpath(notebooks::sheetFolder(dir, 4)));
        touch(notebooks::sheetFolder(dir, 3) + QStringLiteral("/terminada.png"));
        touch(notebooks::sheetFolder(dir, 4) + QStringLiteral("/terminada.png"));
        pages = pagesOf(dir, nb);
        QCOMPARE(pages.size(), 4);
        QCOMPARE(openingPage(pages), 3);
        QCOMPARE(usageText(pages, 4), QStringLiteral("4 de 4 usadas · lleno"));
        QCOMPARE(longDate(QDate(2026, 10, 7)), QStringLiteral("7 oct 2026"));
    }

    // El ajuste, los pasos del zoom y el corrimiento.
    void vista()
    {
        const QSizeF sheet(7680, 4800);
        const QRectF area(24, 24, 1232, 680);
        QCOMPARE(fitScale(sheet, area.size()), 680.0 / 4800);
        // Ajustada: entra entera y centrada en el área.
        const QRectF fit = sheetRect(sheet, area, 1.0, QPointF(500, 500));
        QCOMPARE(fit.height(), 680.0);
        QCOMPARE(fit.center(), area.center());
        // Acercar en pasos de 1,5 hasta un píxel de la hoja por píxel de pantalla; alejar hasta 1.
        QCOMPARE(zoomIn(1.0, sheet, area.size()), 1.5);
        QCOMPARE(maxZoom(sheet, area.size()), 4800.0 / 680);
        double zoom = 1.0;
        for (int i = 0; i < 20; ++i)
            zoom = zoomIn(zoom, sheet, area.size());
        QCOMPARE(zoom, 4800.0 / 680);
        QCOMPARE(zoomOut(1.2), 1.0);
        QCOMPARE(zoomOut(3.0), 2.0);
        // Una hoja chica no se acerca más que su tamaño real.
        QCOMPARE(maxZoom(QSizeF(400, 250), area.size()), 1.0);
        // Acercada, el corrimiento llega hasta el borde de la hoja y no más.
        const QRectF zoomed = sheetRect(sheet, area, 2.0, QPointF(1e6, -1e6));
        QCOMPARE(zoomed.left(), area.left());
        QCOMPARE(zoomed.bottom(), area.bottom());
        QCOMPARE(clampPan(sheet, area, 2.0, QPointF(10, -20)), QPointF(10, -20));
    }

    // La carga en segundo plano: achicada al tamaño pedido, o entera.
    void cargaEnSegundoPlano()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("terminada.png"));
        QImage source(800, 500, QImage::Format_RGB32);
        source.fill(QColor(0xB5, 0x98, 0x78));
        QVERIFY(source.save(path));

        SheetCache cache;
        cache.setBound(QSize(200, 200));
        QStringList ready;
        cache.onReady = [&](const QString& p, bool full, double) { ready.append((full ? QStringLiteral("F ") : QStringLiteral("S ")) + p); };
        QVERIFY(cache.get(path).isNull()); // empieza a cargar
        QTRY_COMPARE(ready.size(), 1);
        QCOMPARE(cache.get(path).size(), QSize(200, 125));
        QVERIFY(cache.get(path, true).isNull());
        QTRY_COMPARE(ready.size(), 2);
        QCOMPARE(cache.peek(path, true).size(), QSize(800, 500));
        // Lo que no se pide más se suelta.
        cache.keepOnly({path});
        QVERIFY(cache.peek(path, true).isNull());
        QVERIFY(!cache.peek(path).isNull());
        cache.keepOnly({});
        QVERIFY(cache.peek(path).isNull());
        // Un archivo que no es una imagen avisa igual, con la imagen vacía.
        QVERIFY(cache.get(tmp.filePath(QStringLiteral("no.png"))).isNull());
        QTRY_COMPARE(ready.size(), 3);
    }
    // HU-138: lo que hace D y cómo vuelve Cartuchera.
    void dibujar()
    {
        QTemporaryDir tmp;
        notebooks::Notebook nb;
        nb.maxSheets = 2;
        Book book{tmp.path(), nb};
        // En blanco: la hoja 1, con --hoja y su carpeta.
        DrawPlan plan = drawPlan(book, QStringLiteral("C:/x/cartuchera.exe"));
        QCOMPARE(plan.action, DrawAction::Launch);
        QCOMPARE(plan.sheet, 1);
        QCOMPARE(plan.args.size(), 2);
        QCOMPARE(plan.args[0], QStringLiteral("--hoja"));
        QCOMPARE(QDir::fromNativeSeparators(plan.args[1]), notebooks::sheetFolder(tmp.path(), 1));
        QCOMPARE(drawPlan(book, QString()).action, DrawAction::NoCartuchera);
        // Sin tocar, borrador, terminada, no la quiso abrir, se cerró de golpe.
        QCOMPARE(drawResult(tmp.path(), 1, 0, false), DrawResult::Untouched);
        QCOMPARE(drawResult(tmp.path(), 1, 2, false), DrawResult::Refused);
        QVERIFY(QDir().mkpath(notebooks::sheetFolder(tmp.path(), 1)));
        QCOMPARE(drawResult(tmp.path(), 1, 0, false), DrawResult::Draft);
        QCOMPARE(drawResult(tmp.path(), 1, 0, true), DrawResult::Crashed);
        // Con borrador, D lo retoma.
        QCOMPARE(drawPlan(book, QStringLiteral("c.exe")).sheet, 1);
        touch(notebooks::sheetFolder(tmp.path(), 1) + QStringLiteral("/terminada.png"));
        QCOMPARE(drawResult(tmp.path(), 1, 0, false), DrawResult::Finished);
        QCOMPARE(drawPlan(book, QStringLiteral("c.exe")).sheet, 2);
        // Lleno: no lanza nada.
        QVERIFY(QDir().mkpath(notebooks::sheetFolder(tmp.path(), 2)));
        touch(notebooks::sheetFolder(tmp.path(), 2) + QStringLiteral("/terminada.png"));
        QCOMPARE(drawPlan(book, QStringLiteral("c.exe")).action, DrawAction::Full);
    }
};

QTEST_MAIN(TestBocetos)
#include "tst_bocetos.moc"
