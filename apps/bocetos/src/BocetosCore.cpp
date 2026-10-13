#include "BocetosCore.h"

#include <appkit/Theme.h>

#include <QDir>
#include <QFileInfo>
#include <QBuffer>
#include <QImageReader>
#include <QDateTime>

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

QString exeOfCommand(const QString& command)
{
    const QString c = command.trimmed();
    if (c.startsWith(QLatin1Char('"'))) {
        const qsizetype end = c.indexOf(QLatin1Char('"'), 1);
        return end > 1 ? c.mid(1, end - 1) : QString();
    }
    const qsizetype exe = c.indexOf(QStringLiteral(".exe"), 0, Qt::CaseInsensitive);
    return exe > 0 ? c.left(exe + 4) : c.section(QLatin1Char(' '), 0, 0);
}

QString findKrita(const QStringList& args, const QString& registryCommand)
{
    const qsizetype at = args.indexOf(QStringLiteral("--krita"));
    if (at >= 0 && at + 1 < args.size())
        return QFileInfo(args[at + 1]).absoluteFilePath();
    for (const QString& path : {exeOfCommand(registryCommand), QStringLiteral("C:/Program Files/Krita (x64)/bin/krita.exe")})
        if (!path.isEmpty() && QFileInfo(path).isFile())
            return QFileInfo(path).absoluteFilePath();
    return QString();
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

const char* const kMonths[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};

QString shortDate(const QDate& date, const QDate& today)
{
    if (!date.isValid())
        return QStringLiteral("?");
    QString out = QStringLiteral("%1 %2").arg(date.day()).arg(QLatin1String(kMonths[date.month() - 1]));
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

QString continuedName(const QString& name)
{
    const QString trimmed = name.trimmed();
    const qsizetype space = trimmed.lastIndexOf(QLatin1Char(' '));
    bool number = false;
    const int n = space > 0 ? trimmed.mid(space + 1).toInt(&number) : 0;
    if (number && n >= 1)
        return QStringLiteral("%1 %2").arg(trimmed.left(space)).arg(n + 1);
    return trimmed + QStringLiteral(" 2");
}

QStringList collectionsOf(const QList<Shelf>& shelves)
{
    QStringList out;
    for (const Shelf& shelf : shelves)
        if (!shelf.collection.isEmpty())
            out.append(shelf.collection);
    std::sort(out.begin(), out.end(), [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });
    return out;
}

Draft draftFor(const Book* selected)
{
    Draft draft;
    draft.cover = appkit::theme::kTapas[0];
    if (!selected)
        return draft;
    draft.collection = selected->notebook.collection;
    if (selected->full) {
        draft.name = continuedName(selected->notebook.name);
        draft.cover = selected->notebook.cover;
        draft.paper = selected->notebook.paper;
        draft.maxSheets = selected->notebook.maxSheets;
        draft.app = selected->notebook.app;
    }
    return draft;
}

notebooks::Notebook notebookFrom(const Draft& draft, const QDate& today)
{
    notebooks::Notebook nb;
    nb.name = draft.name.trimmed();
    nb.collection = draft.collection.trimmed();
    nb.cover = draft.cover;
    nb.paper = draft.paper;
    nb.maxSheets = std::clamp(draft.maxSheets, kMinSheets, kMaxSheets);
    nb.app = draft.app;
    nb.started = today;
    nb.updated = today;
    return nb;
}

int slotAt(const Layout& layout, const QPoint& point)
{
    for (int i = 0; i < layout.items.size(); ++i)
        if (layout.items[i].cover.adjusted(0, 0, 0, kCanto).contains(point))
            return i;
    return -1;
}

QList<Page> pagesOf(const QString& dir, const notebooks::Notebook& notebook)
{
    QList<Page> pages;
    // Se llena en orden: las terminadas son las primeras.
    for (int i = 1; i <= notebook.maxSheets; ++i) {
        const notebooks::SheetState state = notebooks::sheetState(dir, i);
        Page page{i, state, {}, {}};
        if (state == notebooks::SheetState::Finished) {
            page.image = QDir(notebooks::sheetFolder(dir, i)).filePath(QString::fromLatin1(notebooks::kFinishedImage));
            page.finished = QFileInfo(page.image).lastModified().date();
            pages.append(page);
            continue;
        }
        // La siguiente: el aviso; en un cuaderno de Krita, el borrador se ve (HU-142).
        const QString draft = QDir(notebooks::sheetFolder(dir, i)).filePath(QString::fromLatin1(notebooks::kDraftImage));
        if (state == notebooks::SheetState::Draft && notebook.app == notebooks::App::Krita && QFileInfo::exists(draft)) {
            page.image = draft;
            page.finished = QFileInfo(draft).lastModified().date();
        }
        pages.append(page);
        break;
    }
    return pages;
}

int openingPage(const QList<Page>& pages)
{
    for (int i = int(pages.size()) - 1; i >= 0; --i)
        if (pages[i].state == notebooks::SheetState::Finished)
            return i;
    return pages.isEmpty() ? -1 : 0;
}

QString longDate(const QDate& date)
{
    if (!date.isValid())
        return QStringLiteral("?");
    return QStringLiteral("%1 %2 %3").arg(date.day()).arg(QLatin1String(kMonths[date.month() - 1])).arg(date.year());
}

QString usageText(const QList<Page>& pages, int maxSheets)
{
    int used = 0;
    for (const Page& page : pages)
        used += page.state == notebooks::SheetState::Finished;
    QString out = QStringLiteral("%1 de %2 usadas").arg(used).arg(maxSheets);
    if (!pages.isEmpty() && pages.last().state == notebooks::SheetState::Draft)
        out += QStringLiteral(" · borrador en la %1").arg(pages.last().sheet);
    else if (!pages.isEmpty() && pages.last().state == notebooks::SheetState::Empty)
        out += QStringLiteral(" · sigue la %1").arg(pages.last().sheet);
    else
        out += QStringLiteral(" · lleno");
    return out;
}

double fitScale(const QSizeF& sheet, const QSizeF& area)
{
    if (sheet.isEmpty() || area.isEmpty())
        return 1.0;
    return std::min(area.width() / sheet.width(), area.height() / sheet.height());
}

double maxZoom(const QSizeF& sheet, const QSizeF& area)
{
    return std::max(1.0, 1.0 / fitScale(sheet, area));
}

double zoomIn(double zoom, const QSizeF& sheet, const QSizeF& area)
{
    return std::min(zoom * kZoomStep, maxZoom(sheet, area));
}

double zoomOut(double zoom)
{
    return std::max(1.0, zoom / kZoomStep);
}

QPointF clampPan(const QSizeF& sheet, const QRectF& area, double zoom, const QPointF& pan)
{
    const QSizeF size = sheet * (fitScale(sheet, area.size()) * zoom);
    const double freeX = std::max(0.0, (size.width() - area.width()) / 2);
    const double freeY = std::max(0.0, (size.height() - area.height()) / 2);
    return {std::clamp(pan.x(), -freeX, freeX), std::clamp(pan.y(), -freeY, freeY)};
}

QRectF sheetRect(const QSizeF& sheet, const QRectF& area, double zoom, const QPointF& pan)
{
    const QSizeF size = sheet * (fitScale(sheet, area.size()) * zoom);
    QRectF rect(QPointF(), size);
    rect.moveCenter(area.center() + clampPan(sheet, area, zoom, pan));
    return rect;
}

DrawPlan drawPlan(const Book& book, const QString& cartuchera, const QString& krita)
{
    DrawPlan plan;
    const std::optional<int> next = notebooks::nextSheet(book.dir, book.notebook);
    if (!next) {
        plan.action = DrawAction::Full;
        return plan;
    }
    plan.sheet = *next;
    plan.sheetDir = notebooks::sheetFolder(book.dir, *next);
    if (book.notebook.app == notebooks::App::Krita) {
        plan.krita = true;
        plan.kra = QDir(plan.sheetDir).filePath(QString::fromLatin1(notebooks::kKritaFile));
        plan.needsPaper = !QFileInfo::exists(QDir(book.dir).filePath(QString::fromLatin1(notebooks::kPaperImage)));
        if (krita.isEmpty())
            plan.action = DrawAction::NoKrita;
        else if (plan.needsPaper && cartuchera.isEmpty())
            plan.action = DrawAction::NoCartuchera;
        plan.program = krita;
        plan.args = {QStringLiteral("--nosplash"), QDir::toNativeSeparators(plan.kra)};
        return plan;
    }
    if (cartuchera.isEmpty()) {
        plan.action = DrawAction::NoCartuchera;
        return plan;
    }
    plan.program = cartuchera;
    plan.args = {QStringLiteral("--hoja"), QDir::toNativeSeparators(plan.sheetDir)};
    return plan;
}

DrawResult drawResult(const QString& bookDir, int sheet, int exitCode, bool crashed)
{
    const notebooks::SheetState state = notebooks::sheetState(bookDir, sheet);
    if (state == notebooks::SheetState::Finished)
        return DrawResult::Finished; // terminó antes de cualquier otra cosa
    if (crashed)
        return DrawResult::Crashed;
    if (exitCode == 2)
        return DrawResult::Refused;
    return state == notebooks::SheetState::Draft ? DrawResult::Draft : DrawResult::Untouched;
}

DrawResult kritaResult(bool saved, qint64 elapsedMs, bool crashed)
{
    if (saved)
        return DrawResult::Draft; // aunque se haya cerrado de golpe: lo guardado queda
    if (crashed)
        return DrawResult::Crashed;
    return elapsedMs < kKritaQuickMs ? DrawResult::AlreadyOpen : DrawResult::Untouched;
}

QRectF viewerArea(const QSize& window)
{
    return QRectF(kViewerMargin, kViewerMargin, std::max(1, window.width() - kViewerBar - 2 * kViewerMargin),
                  std::max(1, window.height() - 2 * kViewerMargin));
}

QImage tvImage(const Page& page, const QColor& paper, const QSize& bound)
{
    if (!page.image.isEmpty()) {
        QImageReader reader(page.image);
        const QSize size = reader.size();
        if (size.isValid())
            reader.setScaledSize(size.scaled(bound, Qt::KeepAspectRatio).boundedTo(size));
        QImage image = reader.read();
        if (!image.isNull())
            return image;
    }
    QImage blank(QSize(1600, 1000).scaled(bound, Qt::KeepAspectRatio), QImage::Format_RGB32);
    blank.fill(paper.isValid() ? paper : QColor(0xF5, 0xF0, 0xE6));
    return blank;
}

QByteArray jpegOf(const QImage& image, int quality)
{
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPG", quality);
    return out;
}

QUrl tvArgument(const QStringList& args)
{
    const qsizetype at = args.indexOf(QStringLiteral("--tv"));
    return at >= 0 && at + 1 < args.size() ? QUrl(args[at + 1]) : QUrl();
}

} // namespace bocetos
