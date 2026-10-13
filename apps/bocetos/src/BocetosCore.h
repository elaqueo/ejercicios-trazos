#pragma once

#include <notebooks/Library.h>

#include <QByteArray>
#include <QColor>
#include <QDate>
#include <QImage>
#include <QSize>
#include <QUrl>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

// Bocetos: lo que no es pantalla, para probarlo.
namespace bocetos {

// HU-134: la biblioteca de `--biblioteca <carpeta>` (la prueba de la app real, para no tocar los
// cuadernos del usuario); si no está, la de siempre (notebooks::defaultLibraryRoot()).
QString libraryRoot(const QStringList& args);

// HU-134: cartuchera.exe junto a Bocetos (en `appDir`), o vacío si no está.
QString findCartuchera(const QString& appDir);

// HU-135: un libro de la biblioteca: el cuaderno y lo que dice su tapa.
struct Book {
    QString dir;
    notebooks::Notebook notebook;
    int used = 0;       // hojas terminadas
    bool draft = false; // la siguiente hoja está empezada
    bool full = false;  // no queda ninguna: se cierra
};
Book bookOf(const notebooks::Entry& entry);

// Un estante por colección: el general (sin colección) primero y después por nombre; adentro,
// por la última actualización, la más reciente primero (decisión del 13 de octubre de 2026).
struct Shelf {
    QString collection; // vacía: el general
    QList<Book> books;
};
QList<Shelf> shelvesOf(const QList<notebooks::Entry>& entries);
QString shelfTitle(const Shelf& shelf); // "General" o el nombre de la colección

// "desde 1 oct · última 12 oct" (el año, solo si no es el de `today`); sin hojas, solo "desde".
QString bookDates(const Book& book, const QDate& today);

// El acomodo (mesa "Bocetos · biblioteca"): los libros en filas de lo que entre en el ancho, un
// estante debajo del otro, en px de Qt desde arriba del contenido (el encabezado va aparte).
struct Slot {
    int shelf = 0, book = 0; // índices en la lista de estantes
    int row = 0, column = 0; // fila (contando todos los estantes) y columna
    QRect cover;             // la tapa
};
struct ShelfMarks {
    int labelY = 0; // el nombre del estante
    int lineY = 0;  // la línea de la tabla, debajo de su última fila
};
struct Layout {
    QList<Slot> items;       // en el orden de la navegación
    QList<ShelfMarks> marks; // una por estante
    int height = 0;          // del contenido
};
inline constexpr int kMargin = 64, kBookWidth = 232, kBookHeight = 148, kBookGap = 40, kCanto = 12;
Layout layoutLibrary(const QList<Shelf>& shelves, int width);

// Las flechas: ← → el libro anterior o siguiente (de estante a estante); ↑ ↓ la fila de arriba o
// de abajo, en la columna más cercana. En los bordes se queda.
enum class Move { Left, Right, Up, Down };
int moveSelection(const Layout& layout, int current, Move move);

// El libro en `point` (coordenadas del contenido; la tapa con su canto), o -1.
int slotAt(const Layout& layout, const QPoint& point);

// HU-136: lo que se elige en "Cuaderno nuevo" (y en "Editar cuaderno": nombre, colección y tapa).
struct Draft {
    QString name;
    QString collection; // vacía: el estante general
    QColor cover;
    notebooks::Paper paper = notebooks::paperPreset(QStringLiteral("crema"));
    int maxSheets = 40;
};
inline constexpr int kMinSheets = 1, kMaxSheets = 200;

// "Bocetos diarios" → "Bocetos diarios 2"; "Bocetos diarios 2" → "Bocetos diarios 3".
QString continuedName(const QString& name);
// Las colecciones que hay (sin el general), por nombre.
QStringList collectionsOf(const QList<Shelf>& shelves);
// Lo que propone "Cuaderno nuevo": si el elegido está lleno, seguir en uno nuevo (el nombre
// siguiente, el mismo papel, la misma tapa, la misma colección y la misma cantidad de hojas); si no,
// la colección del elegido, la primera tapa, papel crema y 40 hojas.
Draft draftFor(const Book* selected);
// El cuaderno que sale del borrador, empezado y actualizado `today`.
notebooks::Notebook notebookFrom(const Draft& draft, const QDate& today);

// HU-137: el cuaderno abierto. Las páginas que se recorren: las hojas terminadas, en orden, y al
// final la siguiente (el borrador o la primera en blanco) como aviso; un cuaderno lleno no la tiene.
struct Page {
    int sheet = 0; // desde 1
    notebooks::SheetState state = notebooks::SheetState::Empty;
    QString image; // terminada.png (solo las terminadas)
    QDate finished; // la fecha de terminada.png
};
QList<Page> pagesOf(const QString& dir, const notebooks::Notebook& notebook);
// Abre en la última terminada; sin terminadas, en la siguiente (la hoja 1 en blanco).
int openingPage(const QList<Page>& pages);
// "7 oct 2026".
QString longDate(const QDate& date);
// Al pie, a la derecha: "12 de 40 usadas · borrador en la 13" (o "· sigue la 13", o "· lleno").
QString usageText(const QList<Page>& pages, int maxSheets);

// La vista de la hoja: ajustada al área (zoom 1) y acercada en pasos de ×1,5 hasta un píxel de la
// hoja por píxel de pantalla; `pan` corre el centro de la hoja (px de pantalla) y no deja que se
// vea fondo de un lado si la hoja es más grande que el área en ese eje.
inline constexpr double kZoomStep = 1.5;
double fitScale(const QSizeF& sheet, const QSizeF& area);
double maxZoom(const QSizeF& sheet, const QSizeF& area);
double zoomIn(double zoom, const QSizeF& sheet, const QSizeF& area);
double zoomOut(double zoom);
QPointF clampPan(const QSizeF& sheet, const QRectF& area, double zoom, const QPointF& pan);
QRectF sheetRect(const QSizeF& sheet, const QRectF& area, double zoom, const QPointF& pan);

// HU-144: la barra del cuaderno va vertical a la derecha; la hoja, en el resto de la ventana con
// 24 px de margen (mesa "Bocetos · cuaderno abierto").
inline constexpr int kViewerBar = 300, kViewerMargin = 24;
QRectF viewerArea(const QSize& window);

// HU-145: la hoja para el TV: la terminada achicada para que entre en `bound` (3840 × 2400, en un
// TV 4K; DLNA acepta hasta 4096 por lado), o el papel del cuaderno si es la de aviso. En JPEG.
inline constexpr QSize kTvBound{3840, 2400};
QImage tvImage(const Page& page, const QColor& paper, const QSize& bound = kTvBound);
QByteArray jpegOf(const QImage& image, int quality = 90);
// `--tv <url de la descripción>`: ese TV, sin buscar en la red (el TV de mentira del smoke).
QUrl tvArgument(const QStringList& args);

// HU-138: D dibuja la siguiente hoja en Cartuchera (`--hoja`, HU-133).
enum class DrawAction { Launch, Full, NoCartuchera };
struct DrawPlan {
    DrawAction action = DrawAction::Launch;
    int sheet = 0;     // la que se abre (Launch)
    QString sheetDir;  // su carpeta
    QStringList args;  // para cartuchera.exe
};
// Lleno: no lanza nada (se ofrece seguir en uno nuevo); sin Cartuchera: no se puede.
DrawPlan drawPlan(const Book& book, const QString& cartuchera);

// Cómo volvió Cartuchera: la hoja terminada, en borrador, sin tocar (se cerró sin dibujar), no la
// quiso abrir (sale con 2, HU-133) o se cerró de golpe.
enum class DrawResult { Finished, Draft, Untouched, Refused, Crashed };
DrawResult drawResult(const QString& bookDir, int sheet, int exitCode, bool crashed);

} // namespace bocetos
