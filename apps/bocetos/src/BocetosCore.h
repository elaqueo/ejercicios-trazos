#pragma once

#include <notebooks/Library.h>

#include <QDate>
#include <QList>
#include <QPoint>
#include <QRect>
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

} // namespace bocetos
