#include "LibraryWindow.h"

#include "BookExport.h"
#include "Kra.h"
#include "NotebookDialog.h"
#include "NotebookView.h"
#include "Notice.h"
#include "TvChooser.h"
#include "TvLink.h"
#include "Paint.h"
#include "ThumbCache.h"

#include <appkit/ConfirmOverlay.h>

#include <appkit/Theme.h>

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QCloseEvent>

#include <algorithm>
#include <cmath>

#include <windows.h>

namespace theme = appkit::theme;
using bocetos::Book;
using bocetos::drawKey;
using bocetos::themeFont;

namespace {

// Medidas de la mesa "Bocetos · biblioteca" (px a escala 1).
constexpr int kHeaderTop = 48, kContentTop = 150, kFooter = 96, kSpine = 14;
const QColor kTapaTexto{0xF4, 0xF1, 0xEA};
const QColor kCantoLinea{0xDC, 0xD5, 0xC6};

// El canto engorda con las hojas usadas: de 3 a 12 px.
int cantoOf(const Book& book)
{
    return 3 + int(std::lround(9.0 * book.used / std::max(1, book.notebook.maxSheets)));
}

void drawChip(QPainter& p, double x, double y, const QString& text, const QColor& color, bool dot)
{
    const QFont f = themeFont(false, 12);
    const double textW = QFontMetricsF(f).horizontalAdvance(text);
    const double w = textW + 16 + (dot ? 12 : 0);
    const QRectF box(x, y, w, 20);
    QPainterPath path;
    path.addRoundedRect(box, 10, 10);
    p.fillPath(path, QColor(23, 25, 28, 140));
    double tx = x + 8;
    if (dot) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(tx + 3, y + 10), 3, 3);
        tx += 12;
    }
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRectF(tx, y, textW + 2, 20), Qt::AlignLeft | Qt::AlignVCenter, text);
}

// "12 oct" (el año, si no es este).
QString shortDay(const QDate& date)
{
    static const char* const months[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
    if (!date.isValid())
        return QString();
    QString out = QStringLiteral("%1 %2").arg(date.day()).arg(QLatin1String(months[date.month() - 1]));
    if (date.year() != QDate::currentDate().year())
        out += QStringLiteral(" %1").arg(date.year());
    return out;
}

void drawBook(QPainter& p, const Book& book, const QRect& cover, bool selected, const QDate& today)
{
    const int canto = cantoOf(book);
    const QRectF c(cover);
    if (selected) {
        QPainterPath frame;
        frame.addRoundedRect(c.adjusted(-7, -7, 7, canto + 7), 10, 10);
        p.setPen(QPen(theme::kSeleccionTexto, 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(frame);
    }
    // El canto: las hojas debajo de la tapa, rayadas.
    const QRectF edge(c.left() + 4, c.bottom() - 4, c.width() - 6, canto + 4);
    p.save();
    QPainterPath edgePath;
    edgePath.addRoundedRect(edge, 4, 4);
    p.setClipPath(edgePath);
    p.fillRect(edge, theme::kHoja);
    p.setPen(QPen(kCantoLinea, 1));
    for (double y = edge.top() + 2.5; y < edge.bottom(); y += 3)
        p.drawLine(QPointF(edge.left(), y), QPointF(edge.right(), y));
    p.restore();

    // La tapa, con el lomo más oscuro.
    QPainterPath shape;
    shape.addRoundedRect(c, 6, 6);
    p.save();
    p.setClipPath(shape);
    p.fillRect(c, book.notebook.cover.isValid() ? book.notebook.cover : QColor(0x4A, 0x50, 0x59));
    p.fillRect(QRectF(c.left(), c.top(), kSpine, c.height()), QColor(0, 0, 0, 56));
    p.restore();

    const double left = c.left() + 30, width = c.width() - 46;
    const QFont name = themeFont(false, 17, QFont::DemiBold);
    p.setFont(name);
    p.setPen(kTapaTexto);
    const QRectF nameBox(left, c.top() + 14, width, 46);
    const QRectF used = p.boundingRect(nameBox, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, book.notebook.name);
    p.drawText(nameBox, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, book.notebook.name);
    const double chipY = c.top() + 14 + std::min(46.0, used.height()) + 8;
    if (book.draft)
        drawChip(p, left, chipY, QStringLiteral("borrador"), theme::kSeleccionTexto, true);
    else if (book.full)
        drawChip(p, left, chipY, QStringLiteral("lleno"), theme::kTexto, false);
    else if (book.used == 0)
        drawChip(p, left, chipY, QStringLiteral("sin hojas"), theme::kTextoSecundario, false);

    p.setFont(themeFont(true, 11));
    QColor dates = kTapaTexto;
    dates.setAlphaF(0.78f);
    p.setPen(dates);
    p.drawText(QRectF(left, c.bottom() - 28, c.width() - 44, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetricsF(p.font()).elidedText(bocetos::bookDates(book, today), Qt::ElideRight, c.width() - 44));

    p.setFont(themeFont(true, 12));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRectF(c.left(), c.bottom() + canto + 6, c.width(), 18), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1/%2").arg(book.used).arg(book.notebook.maxSheets));
}

} // namespace

LibraryWindow::LibraryWindow(appkit::Shortcuts& shortcuts, appkit::Config& config)
    : m_shortcuts(shortcuts)
    , m_config(config)
{
    setFocusPolicy(Qt::StrongFocus);
    m_view = new NotebookView(m_shortcuts, this);
    m_view->hide();
    m_notice = new Notice(this);
    // HU-145: el TV.
    m_tv = new TvLink(m_config, this);
    m_chooser = new TvChooser(this);
    m_view->onPageShown = [this](const bocetos::Page& page) {
        m_tv->show(page, m_view->book().notebook.paper.color, QStringLiteral("%1 · hoja %2").arg(m_view->book().notebook.name).arg(page.sheet));
    };
    m_view->tvLine = [this] { return NotebookView::TvLine{m_tv->name(), m_tv->status()}; };
    m_tv->onChanged = [this] { m_view->update(); };
    m_tv->onWarn = [this](const QString& title, const QString& detail) { m_notice->warn(title, detail); };
    m_tv->onBusy = [this](const QString& title, const QString& detail) { m_notice->busy(title, detail); };
    m_tv->onIdle = [this] {
        if (m_notice->isBusy() && !m_drawing.sheet)
            m_notice->hide();
    };
    m_tv->onChoose = [this](const QList<cast::Renderer>& renderers) { m_chooser->choose(renderers); };
    m_chooser->onPick = [this](const cast::Renderer& renderer) {
        setFocus();
        m_tv->choose(renderer);
    };
    m_chooser->onCancel = [this] {
        setFocus();
        m_tv->disconnect();
    };
    // HU-143: las miniaturas del elegido.
    m_thumbs = new ThumbCache(this);
    m_thumbs->onReady = [this](const QString& source, double ms, bool built) {
        qInfo().noquote() << QStringLiteral("Miniaturas: %1 %2 en %3 ms")
                                 .arg(QDir::toNativeSeparators(source), built ? QStringLiteral("armada") : QStringLiteral("leída"))
                                 .arg(ms, 0, 'f', 0);
        preview();
    };
    m_dialog = new NotebookDialog(this);
    m_dialog->onCancel = [this] {
        qInfo() << "Bocetos: diálogo cancelado";
        setFocus();
    };
    m_dialog->onAccept = [this](const bocetos::Draft& draft) {
        setFocus();
        if (m_editing.isEmpty()) { // nuevo
            const QString dir = notebooks::createNotebook(m_root, bocetos::notebookFrom(draft, QDate::currentDate()));
            if (dir.isEmpty()) {
                qWarning().noquote() << QStringLiteral("Bocetos: no se pudo crear «%1» en %2").arg(draft.name, m_root);
                return;
            }
            qInfo().noquote() << QStringLiteral("Bocetos: cuaderno nuevo «%1» en %2").arg(draft.name, QDir(dir).dirName());
            reload(QDir(dir).dirName());
            return;
        }
        // Editar: el nombre, la colección y la tapa; la carpeta no cambia.
        std::optional<notebooks::Notebook> nb = notebooks::loadNotebook(m_editing);
        if (!nb) {
            qWarning().noquote() << QStringLiteral("Bocetos: no se pudo leer %1 para editarlo").arg(m_editing);
            return;
        }
        nb->name = draft.name;
        nb->collection = draft.collection;
        nb->cover = draft.cover;
        if (!notebooks::saveNotebook(m_editing, *nb)) {
            qWarning().noquote() << QStringLiteral("Bocetos: no se pudo guardar %1").arg(m_editing);
            return;
        }
        qInfo().noquote() << QStringLiteral("Bocetos: editado «%1» · colección «%2»").arg(nb->name, nb->collection);
        reload(QDir(m_editing).dirName());
    };
    // HU-142: terminar el borrador de Krita (mesa "Bocetos · terminar en Krita").
    m_finish = new appkit::ConfirmOverlay(this);
    m_finish->onCancel = [this] {
        qInfo() << "Krita: terminar cancelado";
        activateWindow();
        setFocus();
    };
    m_finish->onConfirm = [this] {
        activateWindow();
        setFocus();
        const NotebookView* view = openNotebook();
        const bocetos::Page* page = view ? view->currentPage() : nullptr;
        if (!page)
            return;
        const QString dir = view->book().dir;
        const int sheet = page->sheet;
        QString error;
        if (!bocetos::finishKritaSheet(dir, sheet, QDate::currentDate(), &error)) {
            qWarning().noquote() << QStringLiteral("Krita: la hoja %1 no se pudo terminar: %2").arg(sheet).arg(error);
            m_notice->warn(QStringLiteral("La hoja %1 no se pudo terminar").arg(sheet), error);
            return;
        }
        qInfo().noquote() << QStringLiteral("Krita: hoja %1 terminada").arg(sheet);
        m_view->hide();
        m_view->release();
        reload(QDir(dir).dirName());
        openSelected();
        m_view->showSheet(sheet);
    };
    m_confirm = new appkit::ConfirmOverlay(this);
    m_confirm->onCancel = [this] {
        qInfo() << "Bocetos: borrar cancelado";
        activateWindow();
        setFocus();
    };
    m_confirm->onConfirm = [this] {
        activateWindow();
        setFocus();
        const Book* book = selectedBook();
        if (!book)
            return;
        const QString name = book->notebook.name, dir = book->dir;
        // A la Papelera de reciclaje, no borrado para siempre (decidido el 13 de octubre de 2026).
        if (!QFile::moveToTrash(dir)) {
            qWarning().noquote() << QStringLiteral("Bocetos: no se pudo mandar «%1» a la papelera").arg(name);
            return;
        }
        qInfo().noquote() << QStringLiteral("Bocetos: «%1» a la papelera").arg(name);
        // Queda elegido el vecino (el que ocupa su lugar o el anterior).
        const int at = m_selected;
        reload(QString());
        if (!m_layout.items.isEmpty())
            select(std::min(at, int(m_layout.items.size()) - 1), false);
    };
}

bool LibraryWindow::dialogOpen() const
{
    return m_dialog->isVisible() || m_confirm->isVisible() || m_finish->isVisible();
}

void LibraryWindow::setKrita(const QString& krita)
{
    m_krita = krita;
    m_dialog->setKritaFound(!krita.isEmpty());
}

NotebookView* LibraryWindow::openNotebook() const
{
    return m_view->isVisible() ? m_view : nullptr;
}

void LibraryWindow::reload(const QString& selectFolder)
{
    if (!selectFolder.isEmpty())
        m_config.setValue(QStringLiteral("selected"), selectFolder);
    setLibrary(m_root, m_cartuchera);
}

void LibraryWindow::newNotebook()
{
    if (openNotebook())
        return;
    m_editing.clear();
    const Book* selected = selectedBook();
    if (selected && selected->full)
        qInfo().noquote() << QStringLiteral("Bocetos: «%1» está lleno: seguir en uno nuevo").arg(selected->notebook.name);
    m_dialog->open(NotebookDialog::Mode::Create, bocetos::draftFor(selected), bocetos::collectionsOf(m_shelves));
    qInfo() << "Bocetos: cuaderno nuevo (diálogo)";
}

void LibraryWindow::editSelected(bool focusCollection)
{
    if (openNotebook())
        return;
    const Book* book = selectedBook();
    if (!book)
        return;
    m_editing = book->dir;
    bocetos::Draft draft;
    draft.name = book->notebook.name;
    draft.collection = book->notebook.collection;
    draft.cover = book->notebook.cover;
    draft.paper = book->notebook.paper;
    draft.maxSheets = book->notebook.maxSheets;
    draft.app = book->notebook.app;
    m_dialog->open(NotebookDialog::Mode::Edit, draft, bocetos::collectionsOf(m_shelves), focusCollection);
    qInfo().noquote() << QStringLiteral("Bocetos: editar «%1» (diálogo)").arg(book->notebook.name);
}

void LibraryWindow::deleteSelected()
{
    if (openNotebook())
        return;
    const Book* book = selectedBook();
    if (!book)
        return;
    const QString sheets = book->used == 1 ? QStringLiteral("1 hoja terminada") : QStringLiteral("%1 hojas terminadas").arg(book->used);
    m_confirm->ask(QStringLiteral("¿Borrar «%1»?").arg(book->notebook.name),
                   QStringLiteral("Tiene %1. Va a la Papelera de reciclaje.").arg(sheets), Qt::Key_Delete, QStringLiteral("Supr"),
                   QStringLiteral("lo borra"));
    m_confirm->move(mapToGlobal(rect().center() - QPoint(m_confirm->width() / 2, m_confirm->height() / 2)));
    m_confirm->show();
    m_confirm->raise();
    m_confirm->activateWindow();
    m_confirm->setFocus();
    qInfo().noquote() << QStringLiteral("Bocetos: confirmar borrar «%1»").arg(book->notebook.name);
}

void LibraryWindow::setLibrary(const QString& root, const QString& cartuchera)
{
    m_root = root;
    m_cartuchera = cartuchera;
    QElapsedTimer time;
    time.start();
    m_shelves = bocetos::shelvesOf(notebooks::listNotebooks(root));
    int count = 0;
    for (const bocetos::Shelf& shelf : m_shelves)
        count += int(shelf.books.size());
    qInfo().noquote() << QStringLiteral("Biblioteca: %1 cuadernos en %2 estantes, leída en %3 ms")
                             .arg(count)
                             .arg(m_shelves.size())
                             .arg(time.nsecsElapsed() / 1e6, 0, 'f', 1);
    relayout();
    // El último elegido (config "selected": la carpeta del cuaderno); si ya no está, el primero.
    const QString last = m_config.value(QStringLiteral("selected")).toString();
    int slot = m_layout.items.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_layout.items.size(); ++i) {
        const bocetos::Slot& s = m_layout.items[i];
        if (QDir(m_shelves[s.shelf].books[s.book].dir).dirName() == last)
            slot = i;
    }
    select(slot, false);
}

void LibraryWindow::relayout()
{
    m_layout = bocetos::layoutLibrary(m_shelves, gridWidth());
    ensureVisible();
    update();
}

const Book* LibraryWindow::selectedBook() const
{
    if (m_selected < 0 || m_selected >= m_layout.items.size())
        return nullptr;
    const bocetos::Slot& s = m_layout.items[m_selected];
    return &m_shelves[s.shelf].books[s.book];
}

void LibraryWindow::select(int slot, bool log)
{
    if (slot == m_selected && log)
        return;
    m_selected = slot;
    m_preview.clear();
    m_usage.clear();
    if (const Book* book = selectedBook()) {
        m_config.setValue(QStringLiteral("selected"), QDir(book->dir).dirName());
        if (log)
            qInfo().noquote() << QStringLiteral("Bocetos: elegido «%1»").arg(book->notebook.name);
        m_usage = bocetos::usageText(bocetos::pagesOf(book->dir, book->notebook), book->notebook.maxSheets);
        m_preview = bocetos::thumbsOf(book->dir, book->notebook);
        m_previewSince.start();
        m_previewLogged = false;
        preview();
    }
    ensureVisible();
    update();
}

// Pide las miniaturas del elegido que faltan; cuando están todas, lo anota (con cuánto tardó).
void LibraryWindow::preview()
{
    int ready = 0;
    for (const bocetos::Thumb& t : m_preview)
        ready += t.source.isEmpty() || !m_thumbs->get(t.source, t.file).isNull();
    if (!m_previewLogged && ready == m_preview.size()) {
        m_previewLogged = true;
        if (const Book* book = selectedBook())
            qInfo().noquote() << QStringLiteral("Miniaturas: «%1» · %2 listas en %3 ms")
                                     .arg(book->notebook.name)
                                     .arg(ready)
                                     .arg(m_previewSince.nsecsElapsed() / 1e6, 0, 'f', 1);
    }
    update();
}

int LibraryWindow::gridWidth() const
{
    return std::max(1, width() - bocetos::kViewerBar);
}

// HU-143: el panel de la derecha, como la barra del cuaderno abierto: el elegido y sus últimas hojas.
void LibraryWindow::paintPanel(QPainter& p) const
{
    const QRectF bar(width() - bocetos::kViewerBar, 0, bocetos::kViewerBar, height());
    p.fillRect(bar, theme::kPanel);
    p.fillRect(QRectF(bar.left(), 0, 1, height()), theme::kBorde);
    const Book* book = selectedBook();
    if (!book)
        return;
    const double left = bar.left() + 28, inner = bar.width() - 56;
    double y = 32;
    const auto line = [&](const QString& s, const QFont& font, const QColor& color, double h, double x0 = 0) {
        p.setFont(font);
        p.setPen(color);
        p.drawText(QRectF(left + x0, y, inner - x0, h), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetricsF(font).elidedText(s, Qt::ElideRight, inner - x0));
        y += h;
    };
    QFont label = themeFont(false, 12, QFont::DemiBold);
    label.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    line(book->notebook.app == notebooks::App::Krita ? QStringLiteral("CUADERNO · KRITA") : QStringLiteral("CUADERNO"), label,
         theme::kTextoSecundario, 18);
    y += 12;
    const QRectF swatch(left, y, 22, 30);
    p.fillRect(swatch, book->notebook.cover.isValid() ? book->notebook.cover : QColor(0x4A, 0x50, 0x59));
    p.fillRect(QRectF(swatch.left(), swatch.top(), 4, swatch.height()), QColor(0, 0, 0, 56));
    line(book->notebook.name, themeFont(false, 18, QFont::DemiBold), theme::kTexto, 30, 34);
    y += 12;
    for (const QString& part : m_usage.split(QStringLiteral(" · ")))
        line(part, themeFont(true, 13), theme::kTextoSecundario, 20);
    line(bocetos::bookDates(*book, QDate::currentDate()), themeFont(true, 13), theme::kTextoSecundario, 20);
    y += 14;
    p.fillRect(QRectF(left, y, inner, 1), theme::kBorde);
    y += 14;
    line(QStringLiteral("ÚLTIMAS HOJAS"), label, theme::kTextoSecundario, 18);
    y += 10;
    const QColor paper = book->notebook.paper.color.isValid() ? book->notebook.paper.color : theme::kHoja;
    const double tw = inner, th = std::round(inner / 1.6);
    if (m_preview.isEmpty()) {
        const QRectF r(left, y, tw, th);
        p.fillRect(r, paper);
        p.setFont(themeFont(false, 14));
        p.setPen(QColor(0x2A, 0x2A, 0x2A));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Todavía no hay hojas"));
        return;
    }
    for (const bocetos::Thumb& t : m_preview) {
        if (y + th + 26 > height() - 16)
            break; // las que entran
        const QRectF r(left, y, tw, th);
        const QImage image = t.source.isEmpty() ? QImage() : m_thumbs->get(t.source, t.file);
        if (image.isNull())
            p.fillRect(r, paper); // mientras se arma, o un borrador sin imagen: el papel
        else
            p.drawImage(r, image);
        if (t.draft) {
            const QFont f = themeFont(false, 11);
            const double w = QFontMetricsF(f).horizontalAdvance(QStringLiteral("borrador")) + 26;
            const QRectF chip(r.left() + 8, r.top() + 8, w, 20);
            QPainterPath path;
            path.addRoundedRect(chip, 10, 10);
            p.fillPath(path, QColor(23, 25, 28, 184));
            p.setPen(Qt::NoPen);
            p.setBrush(theme::kSeleccionTexto);
            p.drawEllipse(QPointF(chip.left() + 10, chip.center().y()), 2.5, 2.5);
            p.setFont(f);
            p.setPen(theme::kSeleccionTexto);
            p.drawText(chip.adjusted(17, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("borrador"));
        }
        y += th + 6;
        p.setFont(themeFont(true, 12));
        p.setPen(t.draft ? theme::kSeleccionTexto : theme::kTextoSecundario);
        p.drawText(QRectF(left, y, tw, 18), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("hoja %1").arg(t.sheet));
        p.setPen(theme::kTextoSecundario);
        p.drawText(QRectF(left, y, tw, 18), Qt::AlignRight | Qt::AlignVCenter,
                   t.draft ? QStringLiteral("borrador") : shortDay(t.date));
        y += 18 + 14;
    }
}

void LibraryWindow::move(bocetos::Move move)
{
    if (m_layout.items.isEmpty() || openNotebook())
        return;
    select(bocetos::moveSelection(m_layout, m_selected, move));
}

void LibraryWindow::openSelected()
{
    const Book* book = selectedBook();
    if (!book || openNotebook())
        return;
    qInfo().noquote() << QStringLiteral("Bocetos: abrir «%1»").arg(book->notebook.name);
    // HU-142: lo último que se guardó en Krita (también si se guardó con Bocetos ya de vuelta).
    if (book->notebook.app == notebooks::App::Krita && book->draft)
        if (const std::optional<int> next = notebooks::nextSheet(book->dir, book->notebook))
            bocetos::refreshDraft(notebooks::sheetFolder(book->dir, *next));
    m_view->setGeometry(rect());
    m_view->open(*book);
    m_view->show();
    m_view->raise();
    setFocus();
}

void LibraryWindow::draw()
{
    const Book* book = selectedBook();
    if (!book || dialogOpen() || m_drawing.sheet)
        return;
    const bocetos::DrawPlan plan = bocetos::drawPlan(*book, m_cartuchera, m_krita);
    if (plan.action == bocetos::DrawAction::Full) { // seguir en uno nuevo (HU-136)
        if (openNotebook()) {
            m_view->hide();
            m_view->release();
        }
        newNotebook();
        return;
    }
    if (plan.action == bocetos::DrawAction::NoCartuchera) {
        qWarning() << "Bocetos: no se puede dibujar: no está cartuchera.exe junto a bocetos.exe";
        m_notice->warn(QStringLiteral("No se puede dibujar"),
                       plan.krita ? QStringLiteral("El papel de las hojas de Krita lo arma Cartuchera, y no está cartuchera.exe junto a bocetos.exe.")
                                  : QStringLiteral("No está cartuchera.exe junto a bocetos.exe."));
        return;
    }
    if (plan.action == bocetos::DrawAction::NoKrita) {
        qWarning() << "Bocetos: no se puede dibujar: no está Krita";
        m_notice->warn(QStringLiteral("No se puede dibujar"), QStringLiteral("«%1» es un cuaderno de Krita y Krita no está instalado.").arg(book->notebook.name));
        return;
    }
    m_drawing = {book->dir, book->notebook.name, plan.sheet};
    m_drawing.krita = plan.krita;
    m_drawing.kra = plan.kra;
    if (plan.needsPaper)
        preparePaper(plan);
    else
        launch(plan);
}

// HU-142: el papel del cuaderno de Krita lo arma Cartuchera (tiene el motor del papel) una sola vez.
void LibraryWindow::preparePaper(const bocetos::DrawPlan& plan)
{
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, plan](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (code != 0 || status == QProcess::CrashExit) {
            m_drawing.sheet = 0;
            m_notice->hide();
            qWarning().noquote() << QStringLiteral("Bocetos: Cartuchera no armó el papel (código %1)").arg(code);
            m_notice->warn(QStringLiteral("No se pudo preparar el papel"), QStringLiteral("El motivo quedó en cartuchera.log."));
            return;
        }
        qInfo() << "Bocetos: papel listo";
        launch(plan);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        m_drawing.sheet = 0;
        m_notice->warn(QStringLiteral("No arrancó Cartuchera"), process->errorString());
    });
    qInfo().noquote() << QStringLiteral("Bocetos: preparar el papel de «%1»").arg(m_drawing.name);
    m_notice->busy(QStringLiteral("Preparando el papel"), QStringLiteral("%1: el papel de las hojas de Krita, una sola vez.").arg(m_drawing.name));
    process->start(m_cartuchera, {QStringLiteral("--papel"), QDir::toNativeSeparators(m_drawing.dir)});
}

void LibraryWindow::launch(const bocetos::DrawPlan& plan)
{
    const QString app = plan.krita ? QStringLiteral("Krita") : QStringLiteral("Cartuchera");
    if (plan.krita) {
        // La hoja nueva: su carpeta y el .kra con el papel.
        m_drawing.newSheet = !QFileInfo::exists(plan.kra);
        if (m_drawing.newSheet) {
            QString error;
            if (!QDir().mkpath(plan.sheetDir) ||
                !bocetos::writeKra(plan.kra, QDir(m_drawing.dir).filePath(QString::fromLatin1(notebooks::kPaperImage)), &error,
                                     bocetos::kritaProfile(m_krita))) {
                qWarning().noquote() << QStringLiteral("Bocetos: no se pudo escribir %1 (%2)").arg(plan.kra, error);
                QDir(plan.sheetDir).removeRecursively();
                m_drawing.sheet = 0;
                m_notice->hide();
                m_notice->warn(QStringLiteral("No se pudo preparar la hoja %1").arg(plan.sheet), error);
                return;
            }
        }
        m_drawing.before = QFileInfo(plan.kra).lastModified();
        m_drawing.open.start();
    }
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        drawn(code, status == QProcess::CrashExit);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, app](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return; // el resto llega también por finished
        process->deleteLater();
        m_drawing.sheet = 0;
        qWarning().noquote() << QStringLiteral("Bocetos: no arrancó %1 (%2)").arg(app, process->errorString());
        m_notice->warn(QStringLiteral("No arrancó %1").arg(app), process->errorString());
    });
    qInfo().noquote() << QStringLiteral("Bocetos: dibujar «%1» · hoja %2 en %3").arg(m_drawing.name).arg(plan.sheet).arg(app);
    m_notice->busy(QStringLiteral("Dibujando en %1").arg(app),
                   plan.krita ? QStringLiteral("%1 · hoja %2. Guardá con Ctrl+S; al cerrar Krita se vuelve acá y se termina con Ctrl+Enter.")
                                    .arg(m_drawing.name)
                                    .arg(plan.sheet)
                              : QStringLiteral("%1 · hoja %2. Ctrl+Enter la termina; al cerrar Cartuchera se vuelve acá.").arg(m_drawing.name).arg(plan.sheet));
    process->start(plan.program, plan.args);
}

void LibraryWindow::drawn(int exitCode, bool crashed)
{
    const Drawing drawing = m_drawing;
    m_drawing.sheet = 0;
    m_notice->hide();
    bocetos::DrawResult result;
    if (drawing.krita) { // HU-142: lo que se guardó en Krita es el borrador
        const bool saved = QFileInfo(drawing.kra).lastModified() > drawing.before;
        if (saved)
            bocetos::refreshDraft(QFileInfo(drawing.kra).absolutePath());
        result = bocetos::kritaResult(saved, drawing.open.elapsed(), crashed);
        // Una hoja nueva que se cerró sin guardar no deja carpeta: sigue vacía.
        if (drawing.newSheet && (result == bocetos::DrawResult::Untouched || result == bocetos::DrawResult::Crashed)) {
            QDir(QFileInfo(drawing.kra).absolutePath()).removeRecursively();
            qInfo().noquote() << QStringLiteral("Bocetos: hoja %1 cerrada sin guardar: sigue vacía").arg(drawing.sheet);
        }
    } else {
        result = bocetos::drawResult(drawing.dir, drawing.sheet, exitCode, crashed);
    }
    static const char* const names[] = {"terminada", "en borrador", "sin tocar", "no se abrió", "se cerró de golpe", "Krita ya estaba abierto"};
    qInfo().noquote() << QStringLiteral("Bocetos: volvió de %4: hoja %1 %2 (código %3)")
                             .arg(drawing.sheet)
                             .arg(QLatin1String(names[int(result)]))
                             .arg(exitCode)
                             .arg(drawing.krita ? QStringLiteral("Krita") : QStringLiteral("Cartuchera"));
    // Relee el cuaderno (la tapa, las fechas, la hoja nueva) y vuelve a donde estaba; si la hoja
    // quedó terminada, la muestra.
    const bool wasOpen = openNotebook() != nullptr;
    if (wasOpen) {
        m_view->hide();
        m_view->release();
    }
    reload(QDir(drawing.dir).dirName());
    if (result == bocetos::DrawResult::Finished || (drawing.krita && result == bocetos::DrawResult::Draft) || wasOpen) {
        openSelected();
        m_view->showSheet(drawing.sheet);
    }
    if (result == bocetos::DrawResult::Refused)
        m_notice->warn(QStringLiteral("Cartuchera no abrió la hoja %1").arg(drawing.sheet), QStringLiteral("El motivo quedó en cartuchera.log."));
    else if (result == bocetos::DrawResult::AlreadyOpen)
        m_notice->warn(QStringLiteral("Krita ya estaba abierto"),
                       QStringLiteral("La hoja %1 se abrió en ese Krita: dibujala y guardala ahí (Ctrl+S); Bocetos la toma al abrir el cuaderno.")
                           .arg(drawing.sheet));
    else if (result == bocetos::DrawResult::Crashed && drawing.krita)
        m_notice->warn(QStringLiteral("Krita se cerró de golpe"), QStringLiteral("La hoja %1 no se había guardado.").arg(drawing.sheet));
    else if (result == bocetos::DrawResult::Crashed)
        m_notice->warn(QStringLiteral("Cartuchera se cerró de golpe"),
                       notebooks::sheetState(drawing.dir, drawing.sheet) == notebooks::SheetState::Draft
                           ? QStringLiteral("El borrador de la hoja %1 quedó guardado.").arg(drawing.sheet)
                           : QStringLiteral("La hoja %1 quedó en blanco.").arg(drawing.sheet));
    raise();
    activateWindow();
    setFocus();
}

void LibraryWindow::finishSheet()
{
    NotebookView* view = openNotebook();
    if (!view || dialogOpen() || m_drawing.sheet || view->book().notebook.app != notebooks::App::Krita)
        return;
    const bocetos::Page* page = view->currentPage();
    if (!page || page->state != notebooks::SheetState::Draft)
        return;
    const bool blank = !bocetos::refreshDraft(notebooks::sheetFolder(view->book().dir, page->sheet));
    m_finish->ask(blank ? QStringLiteral("¿Terminar la hoja %1 en blanco?").arg(page->sheet) : QStringLiteral("¿Terminar la hoja %1?").arg(page->sheet),
                  blank ? QStringLiteral("Nunca se guardó en Krita: queda el papel solo, cerrada en el cuaderno.")
                        : QStringLiteral("Queda como se ve, cerrada en el cuaderno: ya no se sigue en Krita."),
                  Qt::Key_Return, QStringLiteral("Ctrl+Enter"), QStringLiteral("la termina"), Qt::ControlModifier);
    m_finish->move(mapToGlobal(rect().center() - QPoint(m_finish->width() / 2, m_finish->height() / 2)));
    m_finish->show();
    m_finish->raise();
    m_finish->activateWindow();
    m_finish->setFocus();
    qInfo().noquote() << QStringLiteral("Krita: confirmar terminar la hoja %1%2").arg(page->sheet).arg(blank ? QStringLiteral(" en blanco") : QString());
}

// Dónde guardar: con --exportar-en, ahí sin preguntar; si no, el diálogo de Windows en la última
// carpeta (config "exportDir"; la primera vez, Imágenes). Vacío si se cancela.
QString LibraryWindow::saveFile(const QString& title, const QString& name, const QString& filter)
{
    if (!m_exportDir.isEmpty()) {
        QDir().mkpath(m_exportDir);
        return QDir(m_exportDir).filePath(name);
    }
    QString dir = m_config.value(QStringLiteral("exportDir")).toString();
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
        dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString file = QFileDialog::getSaveFileName(this, title, QDir(dir).filePath(name), filter);
    activateWindow();
    setFocus();
    if (!file.isEmpty())
        m_config.setValue(QStringLiteral("exportDir"), QFileInfo(file).absolutePath());
    return file;
}

void LibraryWindow::exportSheet()
{
    NotebookView* view = openNotebook();
    if (!view || dialogOpen() || m_drawing.sheet || m_exporting)
        return;
    const bocetos::Page* page = view->currentPage();
    if (!page || page->image.isEmpty())
        return; // en blanco: no hay nada que guardar
    const bocetos::Page sheet = *page;
    const QString file = saveFile(QStringLiteral("Guardar la hoja"), bocetos::sheetFileName(view->book().notebook, sheet.sheet),
                                  QStringLiteral("Imagen PNG (*.png)"));
    if (file.isEmpty()) {
        qInfo() << "Exportar: hoja cancelada";
        return;
    }
    QString error;
    if (!bocetos::exportSheet(sheet, file, &error)) {
        qWarning().noquote() << QStringLiteral("Exportar: %1").arg(error);
        m_notice->warn(QStringLiteral("La hoja %1 no se pudo guardar").arg(sheet.sheet), error);
        return;
    }
    qInfo().noquote() << QStringLiteral("Exportar: hoja %1 en %2").arg(sheet.sheet).arg(QDir::toNativeSeparators(file));
    m_notice->info(QStringLiteral("Hoja %1 guardada").arg(sheet.sheet),
                   QStringLiteral("%1, en %2").arg(QFileInfo(file).fileName(), QDir::toNativeSeparators(QFileInfo(file).absolutePath())));
}

void LibraryWindow::exportPdf()
{
    if (dialogOpen() || m_drawing.sheet || m_exporting)
        return;
    const NotebookView* view = openNotebook();
    const Book* selected = selectedBook();
    if (!view && !selected)
        return;
    const Book book = view ? view->book() : *selected;
    const int sheets = int(bocetos::pdfPages(book.dir, book.notebook).size());
    if (sheets == 0) {
        m_notice->warn(QStringLiteral("No hay hojas para el PDF"), QStringLiteral("«%1» todavía no tiene hojas terminadas.").arg(book.notebook.name));
        return;
    }
    const QString file = saveFile(QStringLiteral("Guardar el cuaderno en PDF"), bocetos::pdfFileName(book.notebook), QStringLiteral("PDF (*.pdf)"));
    if (file.isEmpty()) {
        qInfo() << "Exportar: PDF cancelado";
        return;
    }
    m_exporting = true;
    const QString name = book.notebook.name;
    m_notice->busy(QStringLiteral("Armando el PDF"), QStringLiteral("%1 · la tapa").arg(name));
    qInfo().noquote() << QStringLiteral("Exportar: PDF de «%1» (%2 hojas) en %3").arg(name).arg(sheets).arg(QDir::toNativeSeparators(file));
    QElapsedTimer* time = new QElapsedTimer;
    time->start();
    auto* ok = new bool(false);
    auto* error = new QString;
    QThread* thread = QThread::create([this, file, book, name, ok, error] {
        *ok = bocetos::writeNotebookPdf(
            file, book,
            [this, name](int done, int total) {
                QMetaObject::invokeMethod(
                    this, [this, name, done, total] { m_notice->busy(QStringLiteral("Armando el PDF"), QStringLiteral("%1 · hoja %2 de %3").arg(name).arg(done).arg(total)); },
                    Qt::QueuedConnection);
            },
            error);
    });
    connect(thread, &QThread::finished, this, [this, thread, file, sheets, time, ok, error] {
        thread->deleteLater();
        m_exporting = false;
        m_notice->hide();
        const double ms = time->elapsed();
        if (*ok) {
            const double mb = QFileInfo(file).size() / 1e6;
            qInfo().noquote() << QStringLiteral("Exportar: PDF listo, %1 páginas, %2 MB, en %3 ms").arg(sheets + 1).arg(mb, 0, 'f', 1).arg(ms, 0, 'f', 0);
            m_notice->info(QStringLiteral("PDF guardado"), QStringLiteral("%1 · %2 páginas, %3 MB, en %4")
                                                               .arg(QFileInfo(file).fileName())
                                                               .arg(sheets + 1)
                                                               .arg(QLocale(QLocale::Spanish).toString(mb, 'f', 1))
                                                               .arg(QDir::toNativeSeparators(QFileInfo(file).absolutePath())));
        } else {
            qWarning().noquote() << QStringLiteral("Exportar: el PDF no se pudo escribir: %1").arg(*error);
            m_notice->warn(QStringLiteral("El PDF no se pudo guardar"), *error);
        }
        delete time;
        delete ok;
        delete error;
    });
    thread->start();
}

void LibraryWindow::toggleTv()
{
    if (!openNotebook() || dialogOpen() || m_chooser->isVisible())
        return;
    if (const bocetos::Page* page = m_view->currentPage())
        m_tv->show(*page, m_view->book().notebook.paper.color,
                   QStringLiteral("%1 · hoja %2").arg(m_view->book().notebook.name).arg(page->sheet));
    m_tv->toggle();
}

void LibraryWindow::setTvForced(const QUrl& location)
{
    m_tv->setForced(location);
}

void LibraryWindow::closeEvent(QCloseEvent* event)
{
    // Cerrar Bocetos mataría a Cartuchera (QProcess) con la hoja abierta: primero se cierra ella.
    if (m_exporting) { // HU-141: el PDF se está escribiendo en otro hilo
        qInfo() << "Bocetos: no se cierra mientras se arma el PDF";
        event->ignore();
        return;
    }
    if (m_drawing.sheet) {
        qInfo() << "Bocetos: no se cierra mientras se dibuja";
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

void LibraryWindow::closeNotebook()
{
    if (!openNotebook())
        return;
    m_tv->disconnect(); // HU-145: volver a la biblioteca corta el TV
    m_view->hide();
    m_view->release();
    qInfo() << "Bocetos: vuelve a la biblioteca";
    setFocus();
    update();
}

int LibraryWindow::contentTop() const
{
    return kContentTop;
}

int LibraryWindow::viewBottom() const
{
    return height() - kFooter;
}

// Que el elegido (con el nombre del estante, si es la primera fila) quede entre el encabezado y el pie.
void LibraryWindow::ensureVisible()
{
    if (m_selected < 0 || m_selected >= m_layout.items.size()) {
        m_scroll = 0;
        return;
    }
    const QRect cover = m_layout.items[m_selected].cover;
    const int top = cover.top() - 44, bottom = cover.bottom() + bocetos::kCanto + 32;
    const int view = std::max(1, viewBottom() - contentTop());
    if (top - m_scroll < 0)
        m_scroll = std::max(0, top);
    else if (bottom - m_scroll > view)
        m_scroll = bottom - view;
}

void LibraryWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(rect(), theme::kFondo);
    const int left = bocetos::kMargin, right = gridWidth() - bocetos::kMargin;
    const QDate today = QDate::currentDate();

    int books = 0;
    for (const bocetos::Shelf& shelf : m_shelves)
        books += int(shelf.books.size());
    p.setFont(themeFont(false, 26, QFont::DemiBold));
    p.setPen(theme::kTexto);
    p.drawText(QRectF(left, kHeaderTop, right - left, 34), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Bocetos"));
    p.setFont(themeFont(false, 14));
    p.setPen(theme::kTextoSecundario);
    const QString summary = books == 0 ? QStringLiteral("Sin cuadernos todavía")
                                       : QStringLiteral("%1 %2 en %3 %4")
                                             .arg(books)
                                             .arg(books == 1 ? QStringLiteral("cuaderno") : QStringLiteral("cuadernos"))
                                             .arg(m_shelves.size())
                                             .arg(m_shelves.size() == 1 ? QStringLiteral("estante") : QStringLiteral("estantes"));
    p.drawText(QRectF(left, kHeaderTop + 36, right - left, 22), Qt::AlignLeft | Qt::AlignVCenter, summary);

    if (books == 0) { // invita a crear el primero (HU-136)
        p.setFont(themeFont(false, 20, QFont::DemiBold));
        p.setPen(theme::kTexto);
        const QRectF center(0, height() / 2 - 40, width(), 30);
        p.drawText(center, Qt::AlignCenter, QStringLiteral("Todavía no hay cuadernos"));
        p.setFont(themeFont(false, 15));
        p.setPen(theme::kTextoSecundario);
        p.drawText(center.translated(0, 36), Qt::AlignCenter, QStringLiteral("Ctrl+N crea el primero"));
    } else {
        p.save();
        p.setClipRect(QRect(0, contentTop() - 24, width(), viewBottom() - contentTop() + 24));
        p.translate(0, contentTop() - m_scroll);
        for (int s = 0; s < m_shelves.size(); ++s) {
            const bocetos::ShelfMarks& marks = m_layout.marks[s];
            const QString title = bocetos::shelfTitle(m_shelves[s]).toUpper();
            QFont label = themeFont(false, 12, QFont::DemiBold);
            label.setLetterSpacing(QFont::AbsoluteSpacing, 1);
            p.setFont(label);
            p.setPen(theme::kTextoSecundario);
            const double titleW = QFontMetricsF(label).horizontalAdvance(title);
            p.drawText(QRectF(left, marks.labelY, titleW + 2, 18), Qt::AlignLeft | Qt::AlignVCenter, title);
            p.setFont(themeFont(false, 12));
            const int count = int(m_shelves[s].books.size());
            p.drawText(QRectF(left + titleW + 6, marks.labelY, 200, 18), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("· %1 %2").arg(count).arg(count == 1 ? QStringLiteral("cuaderno") : QStringLiteral("cuadernos")));
            p.fillRect(QRect(48, marks.lineY, gridWidth() - 96, 2), theme::kBorde);
        }
        for (int i = 0; i < m_layout.items.size(); ++i) {
            const bocetos::Slot& slot = m_layout.items[i];
            drawBook(p, m_shelves[slot.shelf].books[slot.book], slot.cover, i == m_selected, today);
        }
        p.restore();
    }
    paintPanel(p);

    // El pie: las teclas (y si falta Cartuchera, que no se va a poder dibujar).
    double x = left;
    const double y = height() - 32 - 28;
    p.setFont(themeFont(false, 14));
    const auto hint = [&](const QStringList& keys, const QString& text) {
        for (const QString& k : keys)
            x += drawKey(p, x, y, k) + 6;
        p.setFont(themeFont(false, 14));
        p.setPen(theme::kTextoSecundario);
        const double w = QFontMetricsF(p.font()).horizontalAdvance(text);
        p.drawText(QRectF(x + 2, y, w + 2, 28), Qt::AlignLeft | Qt::AlignVCenter, text);
        x += w + 26;
    };
    if (books > 0) {
        hint({QStringLiteral("←"), QStringLiteral("→"), QStringLiteral("↑"), QStringLiteral("↓")}, QStringLiteral("elegir"));
        hint({QStringLiteral("Enter")}, QStringLiteral("abrir"));
        hint({QStringLiteral("D")}, QStringLiteral("dibujar"));
    }
    hint({QStringLiteral("Ctrl+N")}, QStringLiteral("cuaderno nuevo"));
    if (books > 0)
        hint({QStringLiteral("F2")}, QStringLiteral("editar"));
    hint({QStringLiteral("Ctrl+,")}, QStringLiteral("atajos"));
    hint({QStringLiteral("Ctrl+Q")}, QStringLiteral("salir"));
    if (m_cartuchera.isEmpty()) {
        p.setFont(themeFont(false, 14));
        p.setPen(theme::kSeleccionTexto);
        p.drawText(QRectF(x, y, std::max(0.0, right - x), 28), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("No está cartuchera.exe junto a bocetos.exe: no se va a poder dibujar"));
    }
}

void LibraryWindow::keyPressEvent(QKeyEvent* event)
{
    // El registro va por la tecla virtual de Windows, como en el lienzo. Las flechas repiten.
    if (dialogOpen() || m_drawing.sheet || m_exporting) { // las teclas son del diálogo, o de Cartuchera (o se arma el PDF)
        event->accept();
        return;
    }
    if (m_notice->isVisible() && !m_notice->isBusy()) { // un aviso: cualquier tecla lo saca
        m_notice->hide();
        event->accept();
        return;
    }
    const unsigned key = event->nativeVirtualKey();
    const bool arrow = key >= VK_LEFT && key <= VK_DOWN;
    if ((!event->isAutoRepeat() || arrow) && m_shortcuts.trigger(key, event->modifiers() & Qt::ControlModifier))
        return;
    QWidget::keyPressEvent(event);
}

// Tocar un libro con el lápiz o el mouse lo elige y lo abre.
void LibraryWindow::mousePressEvent(QMouseEvent* event)
{
    if (dialogOpen() || openNotebook())
        return;
    const QPoint content = event->position().toPoint() - QPoint(0, contentTop() - m_scroll);
    if (event->position().y() < contentTop() - 24 || event->position().y() > viewBottom() || event->position().x() >= gridWidth())
        return;
    const int slot = bocetos::slotAt(m_layout, content);
    if (slot < 0)
        return;
    select(slot);
    openSelected();
}

void LibraryWindow::resizeEvent(QResizeEvent*)
{
    if (m_dialog->isVisible())
        m_dialog->setGeometry(rect());
    m_view->setGeometry(rect());
    if (m_notice->isVisible())
        m_notice->setGeometry(rect());
    if (m_chooser->isVisible())
        m_chooser->setGeometry(rect());
    relayout();
}
