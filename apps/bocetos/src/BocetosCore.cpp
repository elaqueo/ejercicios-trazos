#include "BocetosCore.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cstdlib>

namespace bocetos {

QString libraryRoot(const QStringList& args)
{
    const qsizetype at = args.indexOf(QStringLiteral("--biblioteca"));
    if (at >= 0 && at + 1 < args.size())
        return QFileInfo(args[at + 1]).absoluteFilePath();
    return notebooks::defaultLibraryRoot();
}

QString findCartuchera(const QString& appDir)
{
    const QString path = QDir(appDir).filePath(QStringLiteral("cartuchera.exe"));
    return QFileInfo(path).isFile() ? path : QString();
}

Book bookOf(const notebooks::Entry& entry)
{
    Book book{entry.dir, entry.notebook};
    book.used = notebooks::finishedSheets(entry.dir, entry.notebook);
    const std::optional<int> next = notebooks::nextSheet(entry.dir, entry.notebook);
    book.full = !next;
    book.draft = next && notebooks::sheetState(entry.dir, *next) == notebooks::SheetState::Draft;
    return book;
}

QList<Shelf> shelvesOf(const QList<notebooks::Entry>& entries)
{
    QList<Shelf> shelves;
    for (const notebooks::Entry& entry : entries) {
        const QString collection = entry.notebook.collection.trimmed();
        auto it = std::find_if(shelves.begin(), shelves.end(), [&](const Shelf& s) { return s.collection == collection; });
        if (it == shelves.end()) {
            shelves.append({collection, {}});
            it = shelves.end() - 1;
        }
        it->books.append(bookOf(entry));
    }
    std::sort(shelves.begin(), shelves.end(), [](const Shelf& a, const Shelf& b) {
        if (a.collection.isEmpty() != b.collection.isEmpty())
            return a.collection.isEmpty();
        return QString::localeAwareCompare(a.collection, b.collection) < 0;
    });
    for (Shelf& shelf : shelves)
        std::stable_sort(shelf.books.begin(), shelf.books.end(), [](const Book& a, const Book& b) {
            const QDate da = a.notebook.updated.isValid() ? a.notebook.updated : a.notebook.started;
            const QDate db = b.notebook.updated.isValid() ? b.notebook.updated : b.notebook.started;
            return da > db;
        });
    return shelves;
}

QString shelfTitle(const Shelf& shelf)
{
    return shelf.collection.isEmpty() ? QStringLiteral("General") : shelf.collection;
}

namespace {

QString shortDate(const QDate& date, const QDate& today)
{
    static const char* const months[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
    if (!date.isValid())
        return QStringLiteral("?");
    QString out = QStringLiteral("%1 %2").arg(date.day()).arg(QLatin1String(months[date.month() - 1]));
    if (date.year() != today.year())
        out += QStringLiteral(" %1").arg(date.year());
    return out;
}

// Medidas de la mesa (px a escala 1): del nombre del estante a las tapas, 40; cada fila, la tapa,
// el canto y la cuenta; la línea, 46 debajo de la tapa de la última fila; el estante siguiente,
// 52 más abajo.
constexpr int kLabelToBooks = 40, kRowStep = kBookHeight + 56, kCoverToLine = 194, kLineToLabel = 52;

} // namespace

QString bookDates(const Book& book, const QDate& today)
{
    QString out = QStringLiteral("desde ") + shortDate(book.notebook.started, today);
    if (book.used > 0 && book.notebook.updated.isValid())
        out += QStringLiteral(" · última ") + shortDate(book.notebook.updated, today);
    return out;
}

Layout layoutLibrary(const QList<Shelf>& shelves, int width)
{
    Layout layout;
    const int columns = std::max(1, (width - 2 * kMargin + kBookGap) / (kBookWidth + kBookGap));
    int y = 0, row = 0;
    for (int s = 0; s < shelves.size(); ++s) {
        ShelfMarks marks;
        marks.labelY = y;
        int top = y + kLabelToBooks;
        const int count = int(shelves[s].books.size());
        for (int b = 0; b < count; ++b) {
            const int column = b % columns;
            if (b > 0 && column == 0) {
                top += kRowStep;
                ++row;
            }
            layout.items.append({s, b, row, column, QRect(kMargin + column * (kBookWidth + kBookGap), top, kBookWidth, kBookHeight)});
        }
        ++row;
        marks.lineY = top + kCoverToLine;
        layout.marks.append(marks);
        y = marks.lineY + kLineToLabel;
    }
    layout.height = y;
    return layout;
}

int moveSelection(const Layout& layout, int current, Move move)
{
    const int count = int(layout.items.size());
    if (count == 0)
        return -1;
    current = std::clamp(current, 0, count - 1);
    switch (move) {
    case Move::Left:
        return std::max(0, current - 1);
    case Move::Right:
        return std::min(count - 1, current + 1);
    case Move::Up:
    case Move::Down: {
        const Slot& from = layout.items[current];
        const int target = from.row + (move == Move::Up ? -1 : 1);
        int best = current, bestDistance = 1 << 30;
        for (int i = 0; i < count; ++i) {
            if (layout.items[i].row != target)
                continue;
            const int distance = std::abs(layout.items[i].column - from.column);
            if (distance < bestDistance) {
                best = i;
                bestDistance = distance;
            }
        }
        return best;
    }
    }
    return current;
}

int slotAt(const Layout& layout, const QPoint& point)
{
    for (int i = 0; i < layout.items.size(); ++i)
        if (layout.items[i].cover.adjusted(0, 0, 0, kCanto).contains(point))
            return i;
    return -1;
}

} // namespace bocetos
