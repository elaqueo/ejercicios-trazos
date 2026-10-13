#include "BocetosCore.h"
#include "BookExport.h"
#include "Kra.h"
#include "SheetCache.h"
#include "ThumbCache.h"

#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

#include <appkit/Theme.h>

#include <notebooks/Library.h>

#include <QBuffer>
#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
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

// Lo que hace Krita al guardar la hoja: la misma con otra imagen y documentinfo.xml.
void kritaSaves(const QString& kra, const QColor& color)
{
    QList<QPair<QString, QByteArray>> entries;
    {
        QZipReader in(kra);
        QVERIFY(in.isReadable());
        for (const QZipReader::FileInfo& f : in.fileInfoList())
            entries.append({f.filePath, in.fileData(f.filePath)});
    }
    for (auto& [name, data] : entries)
        if (name == QLatin1String("mergedimage.png")) {
            QImage image = QImage::fromData(data);
            image.fill(color);
            data.clear();
            QBuffer buffer(&data);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");
        }
    entries.append({QStringLiteral("documentinfo.xml"), "<document-info/>"});
    QFile file(kra);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QZipWriter zip(&file);
    for (const auto& [name, data] : entries)
        zip.addFile(name, data);
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

    // HU-144: la hoja va a la izquierda de la barra, con su margen, en ventanas chicas y anchas.
    void areaConBarra()
    {
        QCOMPARE(viewerArea(QSize(1280, 800)), QRectF(24, 24, 1280 - 300 - 48, 752));
        for (const QSize window : {QSize(800, 560), QSize(1920, 810), QSize(2560, 1080), QSize(100, 100)}) {
            const QRectF area = viewerArea(window);
            QVERIFY(area.right() <= window.width() - kViewerBar || area.width() == 1);
            QVERIFY(area.width() >= 1 && area.height() >= 1);
            // Ajustada, la hoja nunca pasa debajo de la barra.
            QVERIFY(sheetRect(QSizeF(7680, 4800), area, 1.0, {}).right() <= area.right() + 1e-9);
        }
        // En el UltraWide a pantalla completa la hoja gana alto: la ajusta la altura.
        QCOMPARE(sheetRect(QSizeF(7680, 4800), viewerArea(QSize(2560, 1080)), 1.0, {}).height(), 1032.0);
    }

    // HU-145: la hoja para el TV (achicada a 3840 × 2400, o el papel si es la de aviso), su JPEG y --tv.
    void hojaParaElTv()
    {
        QTemporaryDir tmp;
        const QString png = tmp.filePath(QStringLiteral("terminada.png"));
        QImage sheet(7680, 4800, QImage::Format_RGB32);
        sheet.fill(Qt::white);
        QVERIFY(sheet.save(png));
        Page finished{3, notebooks::SheetState::Finished, png, {}};
        const QImage big = tvImage(finished, Qt::red);
        QCOMPARE(big.size(), QSize(3840, 2400));
        QCOMPARE(big.pixelColor(10, 10), QColor(Qt::white));
        // Una más chica que el TV no se agranda.
        QImage small(800, 500, QImage::Format_RGB32);
        small.fill(Qt::white);
        QVERIFY(small.save(png));
        QCOMPARE(tvImage(finished, Qt::red).size(), QSize(800, 500));
        // La de aviso: el papel, del tamaño del TV.
        const QImage blank = tvImage(Page{4, notebooks::SheetState::Empty, {}, {}}, QColor(0xB5, 0x98, 0x78));
        QCOMPARE(blank.size(), QSize(3840, 2400));
        QCOMPARE(blank.pixelColor(100, 100), QColor(0xB5, 0x98, 0x78));
        const QByteArray jpeg = jpegOf(blank);
        QVERIFY2(!jpeg.isEmpty(), "sin JPEG: falta el plugin qjpeg");
        QCOMPARE(jpeg.left(2).toHex(), QByteArray("ffd8")); // SOI de JPEG
        QCOMPARE(QImage::fromData(jpeg).size(), QSize(3840, 2400));
        QCOMPARE(tvArgument({QStringLiteral("bocetos"), QStringLiteral("--tv"), QStringLiteral("http://127.0.0.1:8799/dmr")}),
                 QUrl(QStringLiteral("http://127.0.0.1:8799/dmr")));
        QVERIFY(!tvArgument({QStringLiteral("bocetos")}).isValid());
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

    // HU-140 y HU-141: los nombres, la hoja como PNG y el PDF.
    void exportar()
    {
        notebooks::Notebook nb;
        nb.name = QStringLiteral("Bocetos: diarios?");
        nb.maxSheets = 40;
        QCOMPARE(sheetFileName(nb, 7), QStringLiteral("Bocetos- diarios- - hoja 07.png"));
        nb.maxSheets = 120;
        QCOMPARE(sheetFileName(nb, 7), QStringLiteral("Bocetos- diarios- - hoja 007.png"));
        nb.name = QStringLiteral("Bocetos diarios");
        QCOMPARE(pdfFileName(nb), QStringLiteral("Bocetos diarios.pdf"));
        QCOMPARE(exportDirArgument({"bocetos.exe", "--exportar-en", "C:/x"}), QStringLiteral("C:/x"));
        QVERIFY(exportDirArgument({"bocetos.exe"}).isEmpty());

        QTemporaryDir tmp;
        nb.maxSheets = 5;
        nb.collection = QStringLiteral("Proyecto xxx");
        nb.cover = QColor(0xA9, 0x53, 0x3D);
        nb.started = QDate(2026, 10, 1);
        nb.updated = QDate(2026, 10, 12);
        QImage sheet(1600, 1000, QImage::Format_RGB32);
        sheet.fill(QColor(0x30, 0x30, 0x30));
        for (int i = 1; i <= 2; ++i) {
            QVERIFY(QDir().mkpath(notebooks::sheetFolder(tmp.path(), i)));
            QVERIFY(sheet.save(QDir(notebooks::sheetFolder(tmp.path(), i)).filePath(QStringLiteral("terminada.png"))));
        }
        QVERIFY(QDir().mkpath(notebooks::sheetFolder(tmp.path(), 3))); // un borrador: no entra
        const QList<Page> pages = pdfPages(tmp.path(), nb);
        QCOMPARE(pages.size(), 2);

        // La hoja: la misma imagen, reemplazando lo que había.
        const QString png = tmp.filePath(QStringLiteral("hoja.png"));
        touch(png);
        QVERIFY(exportSheet(pages[1], png));
        QCOMPARE(QFileInfo(png).size(), QFileInfo(pages[1].image).size());
        QVERIFY(!exportSheet(pagesOf(tmp.path(), nb).last(), png)); // el borrador sin imagen

        // El PDF: la tapa y una página por hoja, del tamaño de la hoja.
        const QString pdf = tmp.filePath(QStringLiteral("cuaderno.pdf"));
        QList<int> progress;
        QVERIFY(writeNotebookPdf(pdf, Book{tmp.path(), nb}, [&](int done, int) { progress.append(done); }));
        QCOMPARE(progress, QList<int>({1, 2}));
        QFile file(pdf);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray data = file.readAll();
        QVERIFY(data.startsWith("%PDF"));
        QCOMPARE(QRegularExpression(QStringLiteral("/Type\\s*/Page[^s]")).globalMatch(QString::fromLatin1(data)).hasNext(), true);
        int count = 0;
        for (auto it = QRegularExpression(QStringLiteral("/Type\\s*/Page[^s]")).globalMatch(QString::fromLatin1(data)); it.hasNext(); it.next())
            ++count;
        QCOMPARE(count, 3);
        const QRegularExpressionMatch box = QRegularExpression(QStringLiteral("/MediaBox \\[0 0 ([\\d.]+) ([\\d.]+)")).match(QString::fromLatin1(data));
        QVERIFY2(box.hasMatch(), data.left(2000).constData());
        QVERIFY2(std::abs(box.captured(1).toDouble() - kPdfPageWidthMm / 25.4 * 72) < 1.5, qPrintable(box.captured(0))); // en puntos
        QVERIFY2(std::abs(box.captured(2).toDouble() - kPdfPageHeightMm / 25.4 * 72) < 1.5, qPrintable(box.captured(0)));
    }

    // HU-143: las últimas hojas del panel y sus miniaturas.
    void miniaturas()
    {
        QTemporaryDir tmp;
        notebooks::Notebook nb;
        nb.maxSheets = 10;
        QVERIFY(thumbsOf(tmp.path(), nb).isEmpty()); // sin hojas
        QImage sheet(1600, 1000, QImage::Format_RGB32);
        sheet.fill(Qt::darkGray);
        for (int i = 1; i <= 4; ++i) {
            QVERIFY(QDir().mkpath(notebooks::sheetFolder(tmp.path(), i)));
            QVERIFY(sheet.save(QDir(notebooks::sheetFolder(tmp.path(), i)).filePath(QStringLiteral("terminada.png"))));
        }
        // Las terminadas, de la más nueva a la más vieja, hasta 3.
        QList<Thumb> thumbs = thumbsOf(tmp.path(), nb);
        QCOMPARE(thumbs.size(), 3);
        QCOMPARE(thumbs[0].sheet, 4);
        QCOMPARE(thumbs[2].sheet, 2);
        QVERIFY(!thumbs[0].draft);
        QCOMPARE(thumbs[0].file, QDir(notebooks::sheetFolder(tmp.path(), 4)).filePath(QStringLiteral("miniatura.png")));
        // Con borrador, va primero; uno de Cartuchera no tiene imagen (el papel).
        QVERIFY(QDir().mkpath(notebooks::sheetFolder(tmp.path(), 5)));
        thumbs = thumbsOf(tmp.path(), nb);
        QCOMPARE(thumbs.size(), 3);
        QCOMPARE(thumbs[0].sheet, 5);
        QVERIFY(thumbs[0].draft);
        QVERIFY(thumbs[0].source.isEmpty());
        QCOMPARE(thumbs[1].sheet, 4);

        // Armarla: entra en 480 × 300 y queda guardada; está al día hasta que la imagen cambia.
        const Thumb& t = thumbs[1];
        QVERIFY(!thumbFresh(t.source, t.file));
        QCOMPARE(writeThumb(t.source, t.file).size(), QSize(480, 300));
        QCOMPARE(QImage(t.file).size(), QSize(480, 300));
        QVERIFY(thumbFresh(t.source, t.file));
        QFile source(t.source);
        QVERIFY(source.open(QIODevice::ReadWrite));
        QVERIFY(source.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
        source.close();
        QVERIFY(!thumbFresh(t.source, t.file));

        // En segundo plano: la primera vez se arma, después sale de la memoria.
        ThumbCache cache;
        QList<bool> built;
        cache.onReady = [&](const QString&, double, bool b) { built.append(b); };
        QVERIFY(cache.get(t.source, t.file).isNull());
        QTRY_COMPARE(built.size(), 1);
        QVERIFY(built[0]);
        QCOMPARE(cache.get(t.source, t.file).size(), QSize(480, 300));
        // Otra (la hoja 3, sin miniatura) se arma; leída de nuevo por otro caché, sale del archivo.
        QVERIFY(cache.get(thumbs[2].source, thumbs[2].file).isNull());
        QTRY_COMPARE(built.size(), 2);
        ThumbCache other;
        QList<bool> again;
        other.onReady = [&](const QString&, double, bool b) { again.append(b); };
        QVERIFY(other.get(thumbs[2].source, thumbs[2].file).isNull());
        QTRY_COMPARE(again.size(), 1);
        QVERIFY(!again[0]);
    }

    // HU-142: D en un cuaderno de Krita, dónde está Krita y cómo vuelve.
    void dibujarEnKrita()
    {
        QTemporaryDir tmp;
        notebooks::Notebook nb;
        nb.maxSheets = 2;
        nb.app = notebooks::App::Krita;
        const Book book{tmp.path(), nb};
        // Sin papel.png: Cartuchera lo arma primero.
        DrawPlan plan = drawPlan(book, QStringLiteral("c.exe"), QStringLiteral("C:/k/krita.exe"));
        QCOMPARE(plan.action, DrawAction::Launch);
        QVERIFY(plan.krita);
        QVERIFY(plan.needsPaper);
        QCOMPARE(plan.program, QStringLiteral("C:/k/krita.exe"));
        QCOMPARE(QDir::fromNativeSeparators(plan.args.last()), QDir(notebooks::sheetFolder(tmp.path(), 1)).filePath(QStringLiteral("hoja.kra")));
        QCOMPARE(plan.kra, QDir::fromNativeSeparators(plan.args.last()));
        QCOMPARE(drawPlan(book, QStringLiteral("c.exe"), QString()).action, DrawAction::NoKrita);
        QCOMPARE(drawPlan(book, QString(), QStringLiteral("k.exe")).action, DrawAction::NoCartuchera);
        // Con el papel, ya no hace falta Cartuchera.
        touch(QDir(tmp.path()).filePath(QStringLiteral("papel.png")));
        plan = drawPlan(book, QString(), QStringLiteral("k.exe"));
        QCOMPARE(plan.action, DrawAction::Launch);
        QVERIFY(!plan.needsPaper);

        QCOMPARE(kritaResult(true, 100, false), DrawResult::Draft);
        QCOMPARE(kritaResult(true, 60000, true), DrawResult::Draft);
        QCOMPARE(kritaResult(false, 60000, false), DrawResult::Untouched);
        QCOMPARE(kritaResult(false, 800, false), DrawResult::AlreadyOpen);
        QCOMPARE(kritaResult(false, 60000, true), DrawResult::Crashed);

        const QString quoted = QStringLiteral("\"C:\\Program Files\\Krita (x64)\\bin\\krita.exe\" \"%1\"");
        QCOMPARE(exeOfCommand(quoted), QStringLiteral("C:\\Program Files\\Krita (x64)\\bin\\krita.exe"));
        QCOMPARE(exeOfCommand(QStringLiteral("C:\\k\\krita.exe %1")), QStringLiteral("C:\\k\\krita.exe"));
        QCOMPARE(findKrita({"bocetos.exe", "--krita", "C:/x/fake_krita.exe"}, QString()), QStringLiteral("C:/x/fake_krita.exe"));
        const QString exe = tmp.filePath(QStringLiteral("krita.exe"));
        touch(exe);
        QCOMPARE(findKrita({"bocetos.exe"}, QStringLiteral("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(exe), QStringLiteral("%1"))), exe);
    }

    // HU-142: la hoja de Krita: el .kra que escribe Bocetos, lo que guarda Krita, el borrador y
    // terminarla.
    void laHojaDeKrita()
    {
        QTemporaryDir tmp;
        notebooks::Notebook nb;
        nb.name = QStringLiteral("Personajes en Krita");
        nb.maxSheets = 3;
        nb.app = notebooks::App::Krita;
        nb.started = nb.updated = QDate(2026, 10, 1);
        QVERIFY(notebooks::saveNotebook(tmp.path(), nb));
        const QString paper = tmp.filePath(QStringLiteral("papel.png"));
        QImage paperImage(160, 100, QImage::Format_RGB32);
        paperImage.fill(QColor(0xB5, 0x98, 0x78));
        paperImage.setText(QString::fromLatin1(kPaperSizeKey), QStringLiteral("640x400")); // la hoja, 4 veces el papel
        QVERIFY(paperImage.save(paper));

        const QString sheet = notebooks::sheetFolder(tmp.path(), 1);
        QVERIFY(QDir().mkpath(sheet));
        const QString kra = QDir(sheet).filePath(QStringLiteral("hoja.kra"));
        QString error;
        QVERIFY2(writeKra(kra, paper, &error), qPrintable(error));
        {
            QZipReader zip(kra);
            QVERIFY(zip.isReadable());
            const QList<QZipReader::FileInfo> files = zip.fileInfoList();
            QCOMPARE(files.first().filePath, QStringLiteral("mimetype")); // primero y sin comprimir
            QFile raw(kra);
            QVERIFY(raw.open(QIODevice::ReadOnly));
            QCOMPARE(raw.read(38 + 19).mid(38), QByteArray("application/x-krita"));
            const QString doc = QString::fromUtf8(zip.fileData(QStringLiteral("maindoc.xml")));
            QVERIFY(doc.contains(QStringLiteral(R"(width="640" height="400")")));
            QVERIFY(doc.contains(QStringLiteral(R"(nodetype="filelayer" source="../papel.png" scale="true" scalingmethod="1")")));
            QVERIFY(doc.contains(QStringLiteral(R"(locked="1")")));
            QVERIFY(doc.contains(QStringLiteral(R"(nodetype="paintlayer")")));
            QCOMPARE(QImage::fromData(zip.fileData(QStringLiteral("preview.png"))).width(), 256);
            // El perfil de la capa (sin él Krita avisa al abrirla).
            QVERIFY(QColorSpace::fromIccProfile(zip.fileData(QStringLiteral("hoja/layers/layer2.icc"))).isValid());
        }
        QVERIFY(!kraSavedByKrita(kra));
        // Sin guardar en Krita no hay borrador: la página es el aviso.
        QVERIFY(!refreshDraft(sheet));
        QList<Page> pages = pagesOf(tmp.path(), nb);
        QCOMPARE(pages.size(), 1);
        QVERIFY(pages[0].image.isEmpty());

        kritaSaves(kra, QColor(0x20, 0x40, 0x60));
        QVERIFY(kraSavedByKrita(kra));
        QVERIFY(refreshDraft(sheet));
        const QString draft = QDir(sheet).filePath(QStringLiteral("borrador.png"));
        QCOMPARE(QImage(draft).pixelColor(5, 5), QColor(0x20, 0x40, 0x60));
        pages = pagesOf(tmp.path(), nb);
        QCOMPARE(pages[0].state, notebooks::SheetState::Draft);
        QCOMPARE(pages[0].image, draft);
        // En un cuaderno de Cartuchera, el borrador no se muestra.
        notebooks::Notebook cartuchera = nb;
        cartuchera.app = notebooks::App::Cartuchera;
        QVERIFY(pagesOf(tmp.path(), cartuchera)[0].image.isEmpty());

        // Terminar: el borrador queda como terminada, el .kra de solo lectura y la fecha al día.
        QVERIFY2(finishKritaSheet(tmp.path(), 1, QDate(2026, 10, 13), &error), qPrintable(error));
        QCOMPARE(notebooks::sheetState(tmp.path(), 1), notebooks::SheetState::Finished);
        QCOMPARE(QImage(QDir(sheet).filePath(QStringLiteral("terminada.png"))).pixelColor(5, 5), QColor(0x20, 0x40, 0x60));
        QVERIFY(!QFileInfo(kra).isWritable());
        QCOMPARE(notebooks::loadNotebook(tmp.path())->updated, QDate(2026, 10, 13));
        QVERIFY(!finishKritaSheet(tmp.path(), 1, QDate(2026, 10, 13)));
        QFile::setPermissions(kra, QFileDevice::ReadOwner | QFileDevice::WriteOwner); // que se pueda borrar

        // Una que nunca se guardó en Krita termina en el papel solo.
        const QString sheet2 = notebooks::sheetFolder(tmp.path(), 2);
        QVERIFY(QDir().mkpath(sheet2));
        QVERIFY(writeKra(QDir(sheet2).filePath(QStringLiteral("hoja.kra")), paper, nullptr, QByteArray("perfil")));
        QCOMPARE(QZipReader(QDir(sheet2).filePath(QStringLiteral("hoja.kra"))).fileData(QStringLiteral("hoja/layers/layer2.icc")), QByteArray("perfil"));
        // El perfil de Krita, de su instalación (../share/color/icc/krita/ desde el exe).
        QVERIFY(kritaProfile(QString()).isEmpty());
        QVERIFY(QDir().mkpath(tmp.filePath(QStringLiteral("k/share/color/icc/krita"))));
        QVERIFY(QDir().mkpath(tmp.filePath(QStringLiteral("k/bin"))));
        QFile icc(tmp.filePath(QStringLiteral("k/share/color/icc/krita/sRGB-elle-V2-srgbtrc.icc")));
        QVERIFY(icc.open(QIODevice::WriteOnly));
        icc.write("icc de krita");
        icc.close();
        QCOMPARE(kritaProfile(tmp.filePath(QStringLiteral("k/bin/krita.exe"))), QByteArray("icc de krita"));
        QVERIFY(finishKritaSheet(tmp.path(), 2, QDate(2026, 10, 13)));
        QCOMPARE(QImage(QDir(sheet2).filePath(QStringLiteral("terminada.png"))).pixelColor(5, 5), QColor(0xB5, 0x98, 0x78));
        QFile::setPermissions(QDir(sheet2).filePath(QStringLiteral("hoja.kra")), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
};

QTEST_MAIN(TestBocetos)
#include "tst_bocetos.moc"
