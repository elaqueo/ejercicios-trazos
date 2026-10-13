#include "LibraryWindow.h"

#include "NotebookDialog.h"
#include "NotebookView.h"
#include "Notice.h"
#include "Paint.h"

#include <appkit/ConfirmOverlay.h>

#include <appkit/Theme.h>

#include <QDir>
#include <QFile>
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
    return m_dialog->isVisible() || m_confirm->isVisible();
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
    m_layout = bocetos::layoutLibrary(m_shelves, width());
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
    if (const Book* book = selectedBook()) {
        m_config.setValue(QStringLiteral("selected"), QDir(book->dir).dirName());
        if (log)
            qInfo().noquote() << QStringLiteral("Bocetos: elegido «%1»").arg(book->notebook.name);
    }
    ensureVisible();
    update();
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
    const bocetos::DrawPlan plan = bocetos::drawPlan(*book, m_cartuchera);
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
        m_notice->warn(QStringLiteral("No se puede dibujar"), QStringLiteral("No está cartuchera.exe junto a bocetos.exe."));
        return;
    }
    m_drawing = {book->dir, book->notebook.name, plan.sheet};
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        drawn(code, status == QProcess::CrashExit);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return; // el resto llega también por finished
        process->deleteLater();
        m_drawing.sheet = 0;
        qWarning().noquote() << QStringLiteral("Bocetos: no arrancó Cartuchera (%1)").arg(process->errorString());
        m_notice->warn(QStringLiteral("No arrancó Cartuchera"), process->errorString());
    });
    qInfo().noquote() << QStringLiteral("Bocetos: dibujar «%1» · hoja %2 en Cartuchera").arg(book->notebook.name).arg(plan.sheet);
    m_notice->busy(QStringLiteral("Dibujando en Cartuchera"),
                   QStringLiteral("%1 · hoja %2. Ctrl+Enter la termina; al cerrar Cartuchera se vuelve acá.").arg(book->notebook.name).arg(plan.sheet));
    process->start(m_cartuchera, plan.args);
}

void LibraryWindow::drawn(int exitCode, bool crashed)
{
    const Drawing drawing = m_drawing;
    m_drawing.sheet = 0;
    m_notice->hide();
    const bocetos::DrawResult result = bocetos::drawResult(drawing.dir, drawing.sheet, exitCode, crashed);
    static const char* const names[] = {"terminada", "en borrador", "sin tocar", "no se abrió", "se cerró de golpe"};
    qInfo().noquote() << QStringLiteral("Bocetos: volvió de Cartuchera: hoja %1 %2 (código %3)")
                             .arg(drawing.sheet)
                             .arg(QLatin1String(names[int(result)]))
                             .arg(exitCode);
    // Relee el cuaderno (la tapa, las fechas, la hoja nueva) y vuelve a donde estaba; si la hoja
    // quedó terminada, la muestra.
    const bool wasOpen = openNotebook() != nullptr;
    if (wasOpen) {
        m_view->hide();
        m_view->release();
    }
    reload(QDir(drawing.dir).dirName());
    if (result == bocetos::DrawResult::Finished || wasOpen) {
        openSelected();
        m_view->showSheet(drawing.sheet);
    }
    if (result == bocetos::DrawResult::Refused)
        m_notice->warn(QStringLiteral("Cartuchera no abrió la hoja %1").arg(drawing.sheet), QStringLiteral("El motivo quedó en cartuchera.log."));
    else if (result == bocetos::DrawResult::Crashed)
        m_notice->warn(QStringLiteral("Cartuchera se cerró de golpe"),
                       notebooks::sheetState(drawing.dir, drawing.sheet) == notebooks::SheetState::Draft
                           ? QStringLiteral("El borrador de la hoja %1 quedó guardado.").arg(drawing.sheet)
                           : QStringLiteral("La hoja %1 quedó en blanco.").arg(drawing.sheet));
    raise();
    activateWindow();
    setFocus();
}

void LibraryWindow::closeEvent(QCloseEvent* event)
{
    // Cerrar Bocetos mataría a Cartuchera (QProcess) con la hoja abierta: primero se cierra ella.
    if (m_drawing.sheet) {
        qInfo() << "Bocetos: no se cierra mientras se dibuja en Cartuchera";
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

void LibraryWindow::closeNotebook()
{
    if (!openNotebook())
        return;
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
    const int left = bocetos::kMargin, right = width() - bocetos::kMargin;
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
            p.fillRect(QRect(48, marks.lineY, width() - 96, 2), theme::kBorde);
        }
        for (int i = 0; i < m_layout.items.size(); ++i) {
            const bocetos::Slot& slot = m_layout.items[i];
            drawBook(p, m_shelves[slot.shelf].books[slot.book], slot.cover, i == m_selected, today);
        }
        p.restore();
    }

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
    if (dialogOpen() || m_drawing.sheet) { // las teclas son del diálogo, o de Cartuchera
        event->accept();
        return;
    }
    if (m_notice->isVisible()) { // un aviso: cualquier tecla lo saca
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
    if (event->position().y() < contentTop() - 24 || event->position().y() > viewBottom())
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
    relayout();
}
