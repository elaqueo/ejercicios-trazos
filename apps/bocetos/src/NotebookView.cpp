#include "NotebookView.h"

#include "Paint.h"

#include <appkit/Shortcuts.h>
#include <appkit/Theme.h>

#include <QFontMetricsF>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include <algorithm>
#include <cmath>

namespace theme = appkit::theme;
using bocetos::drawKey;
using bocetos::Page;
using bocetos::themeFont;
using notebooks::SheetState;

namespace {

// Medidas de la mesa "Bocetos · cuaderno abierto" (px a escala 1): la hoja con 24 de margen y el
// pie de 96 debajo.
constexpr int kMargin = 24, kFooter = 96;

} // namespace

NotebookView::NotebookView(appkit::Shortcuts& shortcuts, QWidget* parent)
    : QWidget(parent)
    , m_shortcuts(shortcuts)
{
    setFocusPolicy(Qt::NoFocus); // las teclas siguen yendo a la ventana
    m_cache.onReady = [this](const QString& path, bool full, double ms) { sheetReady(path, full, ms); };
}

void NotebookView::open(const bocetos::Book& book)
{
    m_book = book;
    m_pages = bocetos::pagesOf(book.dir, book.notebook);
    m_sizes.clear();
    m_nominal = QSize(1600, 1000);
    for (int i = int(m_pages.size()) - 1; i >= 0; --i)
        if (m_pages[i].state == SheetState::Finished) {
            const QSize size = QImageReader(m_pages[i].image).size();
            if (size.isValid())
                m_nominal = size;
            break;
        }
    // Las achicadas, al tamaño del monitor: agrandar la ventana no las vuelve a leer.
    const QScreen* on = screen();
    m_cache.setBound(on ? (on->size() * on->devicePixelRatio()) : QSize(1920, 1080));
    m_cache.keepOnly({});
    m_page = -1;
    qInfo().noquote() << QStringLiteral("Cuaderno: abierto «%1» · %2").arg(book.notebook.name, bocetos::usageText(m_pages, book.notebook.maxSheets));
    showPage(bocetos::openingPage(m_pages));
}

void NotebookView::release()
{
    m_cache.keepOnly({});
    m_page = -1;
}

void NotebookView::showSheet(int sheet)
{
    for (int i = 0; i < m_pages.size(); ++i)
        if (m_pages[i].sheet == sheet)
            showPage(i);
}

void NotebookView::step(int delta)
{
    showPage(std::clamp(m_page + delta, 0, int(m_pages.size()) - 1));
}

void NotebookView::first()
{
    showPage(0);
}

void NotebookView::last()
{
    showPage(int(m_pages.size()) - 1);
}

void NotebookView::showPage(int page)
{
    if (page < 0 || page >= m_pages.size() || page == m_page)
        return;
    m_page = page;
    m_zoom = 1.0;
    m_pan = {};
    m_dragging = false;
    unsetCursor();
    m_since.start();
    m_waiting = true;
    m_how.clear();
    const Page& p = m_pages[page];
    if (p.state == SheetState::Finished && !m_sizes.contains(p.image))
        m_sizes.insert(p.image, QImageReader(p.image).size());
    request();
    update();
}

void NotebookView::request()
{
    if (m_page < 0)
        return;
    const Page& current = m_pages[m_page];
    // La actual y las vecinas (la siguiente, la anterior y la de después); el resto se suelta.
    QStringList wanted;
    for (int d : {0, 1, -1, 2}) {
        const int i = m_page + d;
        if (i >= 0 && i < m_pages.size() && m_pages[i].state == SheetState::Finished)
            wanted.append(m_pages[i].image);
    }
    const bool full = needFull();
    m_cache.keepOnly(wanted, full ? current.image : QString());
    if (m_waiting && current.state == SheetState::Finished && !m_cache.get(current.image).isNull())
        m_how = QStringLiteral("precargada");
    if (full)
        m_cache.get(current.image, true);
    for (const QString& path : wanted)
        m_cache.get(path);
}

void NotebookView::sheetReady(const QString& path, bool full, double ms)
{
    if (m_page < 0 || m_pages[m_page].image != path)
        return; // una vecina: queda lista
    if (!full && m_waiting) {
        m_how = QStringLiteral("leída en %1 ms").arg(ms, 0, 'f', 0);
    } else if (full) {
        qInfo().noquote() << QStringLiteral("Cuaderno: hoja %1 entera, leída en %2 ms").arg(m_pages[m_page].sheet).arg(ms, 0, 'f', 0);
    }
    update();
}

void NotebookView::logShown(const QString& how)
{
    const Page& p = m_pages[m_page];
    const double ms = m_since.nsecsElapsed() / 1e6;
    if (p.state == SheetState::Finished)
        qInfo().noquote() << QStringLiteral("Cuaderno: hoja %1 en %2 ms (%3)").arg(p.sheet).arg(ms, 0, 'f', 1).arg(how);
    else
        qInfo().noquote() << QStringLiteral("Cuaderno: hoja %1 · %2").arg(p.sheet).arg(p.state == SheetState::Draft ? QStringLiteral("borrador") : QStringLiteral("en blanco"));
}

QRectF NotebookView::area() const
{
    return QRectF(kMargin, kMargin, std::max(1, width() - 2 * kMargin), std::max(1, height() - kMargin - kFooter));
}

QSizeF NotebookView::sheetSize() const
{
    if (m_page >= 0 && m_pages[m_page].state == SheetState::Finished) {
        const QSize size = m_sizes.value(m_pages[m_page].image);
        if (size.isValid())
            return size;
    }
    return m_nominal;
}

bool NotebookView::needFull() const
{
    if (m_page < 0 || m_pages[m_page].state != SheetState::Finished || m_zoom <= 1.0)
        return false;
    const QSizeF sheet = sheetSize();
    const double shown = bocetos::sheetRect(sheet, area(), m_zoom, m_pan).width() * devicePixelRatioF();
    const double scaled = std::min(sheet.width(), QSizeF(sheet).scaled(m_cache.bound(), Qt::KeepAspectRatio).width());
    return shown > scaled * 1.05;
}

void NotebookView::setZoom(double zoom)
{
    if (m_page < 0 || std::abs(zoom - m_zoom) < 1e-6)
        return;
    m_pan *= zoom / m_zoom; // hacia el centro
    m_zoom = zoom;
    m_pan = bocetos::clampPan(sheetSize(), area(), m_zoom, m_pan);
    if (m_zoom > 1.0)
        setCursor(Qt::OpenHandCursor);
    else
        unsetCursor();
    qInfo().noquote() << QStringLiteral("Cuaderno: zoom %1 %").arg(std::lround(m_zoom * 100));
    request();
    update();
}

void NotebookView::zoomIn()
{
    setZoom(bocetos::zoomIn(m_zoom, sheetSize(), area().size()));
}

void NotebookView::zoomOut()
{
    setZoom(bocetos::zoomOut(m_zoom));
}

void NotebookView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), theme::kFondo);
    if (m_page < 0)
        return;
    const Page& page = m_pages[m_page];
    const QRectF area = this->area();
    const QSizeF sheet = sheetSize();
    const QRectF shown = bocetos::sheetRect(sheet, area, m_zoom, m_pan);
    const QColor paper = m_book.notebook.paper.color.isValid() ? m_book.notebook.paper.color : theme::kHoja;

    p.save();
    p.setClipRect(area);
    QImage image;
    if (page.state == SheetState::Finished) {
        image = needFull() ? m_cache.peek(page.image, true) : QImage();
        if (image.isNull())
            image = m_cache.peek(page.image);
    }
    if (image.isNull())
        p.fillRect(shown, paper); // en blanco, o mientras carga: el papel del cuaderno
    else
        p.drawImage(shown, image);
    // RNF-02: cuánto tardó en verse, desde la tecla hasta este pintado.
    if (m_waiting && (!image.isNull() || page.state != SheetState::Finished)) {
        m_waiting = false;
        logShown(m_how);
    }
    if (page.state != SheetState::Finished) {
        QColor ink(0x2A, 0x2A, 0x2A);
        p.setFont(themeFont(false, 22, QFont::DemiBold));
        p.setPen(ink);
        const QRectF mid(shown.left(), shown.center().y() - 34, shown.width(), 32);
        const bool none = m_page == 0;
        p.drawText(mid, Qt::AlignCenter,
                   page.state == SheetState::Draft ? QStringLiteral("Hoja %1 · borrador").arg(page.sheet)
                                                   : QStringLiteral("Hoja %1 · en blanco").arg(page.sheet));
        ink.setAlphaF(0.65f);
        p.setPen(ink);
        p.setFont(themeFont(false, 16));
        p.drawText(mid.translated(0, 40), Qt::AlignCenter,
                   page.state == SheetState::Draft ? QStringLiteral("Empezada en Cartuchera: se sigue desde ahí")
                   : none                          ? QStringLiteral("Todavía no hay hojas: dibujá la primera en Cartuchera")
                                                   : QStringLiteral("La siguiente que vas a dibujar"));
    }
    p.restore();

    // El pie, alineado con la hoja ajustada.
    const QRectF fit = bocetos::sheetRect(sheet, area, 1.0, {});
    const double left = fit.left(), right = fit.right();
    const double y = area.bottom() + 18;
    double x = left;
    const auto text = [&](const QString& s, const QFont& font, const QColor& color) {
        p.setFont(font);
        p.setPen(color);
        const double w = QFontMetricsF(font).horizontalAdvance(s);
        p.drawText(QRectF(x, y, w + 2, 22), Qt::AlignLeft | Qt::AlignVCenter, s);
        x += w + 12;
    };
    text(m_book.notebook.name, themeFont(false, 16, QFont::DemiBold), theme::kTexto);
    int finished = 0;
    for (const Page& pg : m_pages)
        finished += pg.state == SheetState::Finished;
    if (page.state == SheetState::Finished) {
        text(QStringLiteral("hoja %1 de %2").arg(page.sheet).arg(finished), themeFont(true, 14), theme::kSeleccionTexto);
        text(QStringLiteral("terminada el %1").arg(bocetos::longDate(page.finished)), themeFont(false, 14), theme::kTextoSecundario);
    } else {
        text(QStringLiteral("hoja %1").arg(page.sheet), themeFont(true, 14), theme::kSeleccionTexto);
        text(page.state == SheetState::Draft ? QStringLiteral("borrador") : QStringLiteral("en blanco"), themeFont(false, 14),
             theme::kTextoSecundario);
    }
    p.setFont(themeFont(true, 13));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRectF(x, y, std::max(0.0, right - x), 22), Qt::AlignRight | Qt::AlignVCenter,
               bocetos::usageText(m_pages, m_book.notebook.maxSheets));

    // Las teclas, del registro.
    x = left;
    const double ky = height() - 20 - 28;
    const auto hint = [&](const QStringList& names, const QString& what, bool dot) {
        for (const QString& name : names)
            x += drawKey(p, x, ky, m_shortcuts.keyOf(name)) + 6;
        p.setFont(themeFont(false, 14));
        p.setPen(theme::kTextoSecundario);
        const QString s = dot ? what + QStringLiteral("  ·") : what;
        const double w = QFontMetricsF(p.font()).horizontalAdvance(s);
        p.drawText(QRectF(x + 2, ky, w + 2, 28), Qt::AlignLeft | Qt::AlignVCenter, s);
        x += w + 18;
    };
    hint({QStringLiteral("Hoja anterior"), QStringLiteral("Hoja siguiente")}, QStringLiteral("hojas"), true);
    hint({QStringLiteral("Primera hoja"), QStringLiteral("Última hoja")}, QStringLiteral("primera / última"), true);
    hint({QStringLiteral("Acercar"), QStringLiteral("Alejar")},
         m_zoom > 1.0 ? QStringLiteral("zoom %1 %").arg(std::lround(m_zoom * 100)) : QStringLiteral("zoom"), true);
    if (m_pages.last().state != SheetState::Finished) // HU-138; lleno, no hay siguiente
        hint({QStringLiteral("Dibujar la siguiente hoja")}, QStringLiteral("dibujar la hoja %1").arg(m_pages.last().sheet), true);
    hint({QStringLiteral("Volver a la biblioteca")}, QStringLiteral("biblioteca"), false);
}

// Arrastrar con el mouse o el lápiz mueve la hoja acercada.
void NotebookView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_zoom <= 1.0)
        return;
    m_dragging = true;
    m_dragStart = event->position();
    m_panStart = m_pan;
    setCursor(Qt::ClosedHandCursor);
}

void NotebookView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging)
        return;
    m_pan = bocetos::clampPan(sheetSize(), area(), m_zoom, m_panStart + (event->position() - m_dragStart));
    update();
}

void NotebookView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_dragging || event->button() != Qt::LeftButton)
        return;
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
    qInfo().noquote() << QStringLiteral("Cuaderno: hoja corrida %1, %2").arg(std::lround(m_pan.x())).arg(std::lround(m_pan.y()));
}
