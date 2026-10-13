#include "NotebookView.h"

#include "Paint.h"

#include <appkit/Shortcuts.h>
#include <appkit/Theme.h>

#include <QFontMetricsF>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#include <algorithm>
#include <cmath>

namespace theme = appkit::theme;
using bocetos::drawKey;
using bocetos::Page;
using bocetos::themeFont;
using notebooks::SheetState;

namespace {


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
    const QRectF a = area();
    qInfo().noquote() << QStringLiteral("Cuaderno: abierto «%1» · %2 · hoja en %3,%4 %5 x %6, barra desde x %7")
                             .arg(book.notebook.name, bocetos::usageText(m_pages, book.notebook.maxSheets))
                             .arg(a.left())
                             .arg(a.top())
                             .arg(a.width())
                             .arg(a.height())
                             .arg(width() - bocetos::kViewerBar);
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
    if (!p.image.isEmpty() && !m_sizes.contains(p.image))
        m_sizes.insert(p.image, QImageReader(p.image).size());
    request();
    update();
    if (onPageShown)
        onPageShown(p);
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
        if (i >= 0 && i < m_pages.size() && !m_pages[i].image.isEmpty())
            wanted.append(m_pages[i].image);
    }
    const bool full = needFull();
    m_cache.keepOnly(wanted, full ? current.image : QString());
    if (m_waiting && !current.image.isEmpty() && !m_cache.get(current.image).isNull())
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
    if (!p.image.isEmpty())
        qInfo().noquote() << QStringLiteral("Cuaderno: hoja %1%4 en %2 ms (%3)")
                                 .arg(p.sheet)
                                 .arg(ms, 0, 'f', 1)
                                 .arg(how, p.state == SheetState::Draft ? QStringLiteral(" (borrador de Krita)") : QString());
    else
        qInfo().noquote() << QStringLiteral("Cuaderno: hoja %1 · %2").arg(p.sheet).arg(p.state == SheetState::Draft ? QStringLiteral("borrador") : QStringLiteral("en blanco"));
}

QRectF NotebookView::area() const
{
    return bocetos::viewerArea(size());
}

QSizeF NotebookView::sheetSize() const
{
    if (m_page >= 0 && !m_pages[m_page].image.isEmpty()) {
        const QSize size = m_sizes.value(m_pages[m_page].image);
        if (size.isValid())
            return size;
    }
    return m_nominal;
}

bool NotebookView::needFull() const
{
    if (m_page < 0 || m_pages[m_page].image.isEmpty() || m_zoom <= 1.0)
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
    if (!page.image.isEmpty()) {
        image = needFull() ? m_cache.peek(page.image, true) : QImage();
        if (image.isNull())
            image = m_cache.peek(page.image);
    }
    if (image.isNull())
        p.fillRect(shown, paper); // en blanco, o mientras carga: el papel del cuaderno
    else
        p.drawImage(shown, image);
    // RNF-02: cuánto tardó en verse, desde la tecla hasta este pintado.
    if (m_waiting && (!image.isNull() || page.image.isEmpty())) {
        m_waiting = false;
        logShown(m_how);
    }
    const bool krita = m_book.notebook.app == notebooks::App::Krita;
    const QString app = krita ? QStringLiteral("Krita") : QStringLiteral("Cartuchera");
    if (page.state == SheetState::Draft && !page.image.isEmpty()) {
        // HU-142: el borrador de Krita se ve, con la marca arriba a la izquierda (mesa "Bocetos ·
        // cuaderno de Krita").
        const QFont f = themeFont(false, 13);
        const QString text = QStringLiteral("borrador");
        const double w = QFontMetricsF(f).horizontalAdvance(text) + 32;
        const QRectF chip(std::max(shown.left(), area.left()) + 16, std::max(shown.top(), area.top()) + 14, w, 24);
        QPainterPath path;
        path.addRoundedRect(chip, 12, 12);
        p.fillPath(path, QColor(23, 25, 28, 184));
        p.setPen(Qt::NoPen);
        p.setBrush(theme::kSeleccionTexto);
        p.drawEllipse(QPointF(chip.left() + 13, chip.center().y()), 3, 3);
        p.setFont(f);
        p.setPen(theme::kSeleccionTexto);
        p.drawText(chip.adjusted(22, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
    } else if (page.state != SheetState::Finished) {
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
                   page.state == SheetState::Draft ? QStringLiteral("Empezada en %1: se sigue desde ahí").arg(app)
                   : none                          ? QStringLiteral("Todavía no hay hojas: dibujá la primera en %1").arg(app)
                                                   : QStringLiteral("La siguiente que vas a dibujar"));
    }
    p.restore();

    // La barra (HU-144): vertical a la derecha, como el aviso de Cartuchera en modo cuaderno.
    const QRectF bar(width() - bocetos::kViewerBar, 0, bocetos::kViewerBar, height());
    p.fillRect(bar, theme::kPanel);
    p.fillRect(QRectF(bar.left(), 0, 1, height()), theme::kBorde);
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
    line(krita ? QStringLiteral("CUADERNO · KRITA") : QStringLiteral("CUADERNO"), label, theme::kTextoSecundario, 18);
    y += 12;
    const QRectF swatch(left, y, 22, 30);
    p.fillRect(swatch, m_book.notebook.cover.isValid() ? m_book.notebook.cover : QColor(0x4A, 0x50, 0x59));
    p.fillRect(QRectF(swatch.left(), swatch.top(), 4, swatch.height()), QColor(0, 0, 0, 56));
    line(m_book.notebook.name, themeFont(false, 18, QFont::DemiBold), theme::kTexto, 30, 34);
    y += 18;
    int finished = 0;
    for (const Page& pg : m_pages)
        finished += pg.state == SheetState::Finished;
    if (page.state == SheetState::Finished) {
        line(QStringLiteral("hoja %1 de %2").arg(page.sheet).arg(finished), themeFont(true, 15), theme::kSeleccionTexto, 22);
        line(QStringLiteral("terminada el %1").arg(bocetos::longDate(page.finished)), themeFont(false, 14), theme::kTextoSecundario, 22);
    } else {
        line(QStringLiteral("hoja %1").arg(page.sheet), themeFont(true, 15), theme::kSeleccionTexto, 22);
        const QString state = page.state != SheetState::Draft ? QStringLiteral("en blanco")
                              : page.image.isEmpty()        ? QStringLiteral("borrador")
                                                            : QStringLiteral("borrador del %1").arg(bocetos::longDate(page.finished));
        line(state, themeFont(false, 14), theme::kTextoSecundario, 22);
    }
    y += 18;
    p.fillRect(QRectF(left, y, inner, 1), theme::kBorde);
    y += 14;
    for (const QString& part : bocetos::usageText(m_pages, m_book.notebook.maxSheets).split(QStringLiteral(" · ")))
        line(part, themeFont(true, 13), theme::kTextoSecundario, 20);
    // HU-145: el TV, si está conectado (o buscando).
    const TvLine tv = tvLine ? tvLine() : TvLine{};
    const bool tvOn = !tv.status.isEmpty();
    if (tvOn) {
        y += 18;
        p.fillRect(QRectF(left, y, inner, 1), theme::kBorde);
        y += 14;
        line(QStringLiteral("TV"), label, theme::kTextoSecundario, 18);
        y += 4;
        if (!tv.name.isEmpty()) {
            p.setPen(Qt::NoPen);
            p.setBrush(theme::kGuardado);
            p.drawEllipse(QPointF(left + 4, y + 11), 4, 4);
            line(tv.name, themeFont(false, 14), theme::kTexto, 22, 16);
        }
        line(tv.status, themeFont(true, 13), theme::kTextoSecundario, 20);
    }

    // Las teclas, del registro, una debajo de la otra al pie de la barra.
    struct Hint {
        QStringList names;
        QString what;
    };
    QList<Hint> hints{{{QStringLiteral("Hoja anterior"), QStringLiteral("Hoja siguiente")}, QStringLiteral("hojas")},
                      {{QStringLiteral("Primera hoja"), QStringLiteral("Última hoja")}, QStringLiteral("primera / última")},
                      {{QStringLiteral("Acercar"), QStringLiteral("Alejar")},
                       m_zoom > 1.0 ? QStringLiteral("zoom %1 %").arg(std::lround(m_zoom * 100)) : QStringLiteral("zoom")}};
    if (m_pages.last().state != SheetState::Finished) // HU-138; lleno, no hay siguiente
        hints.append({{QStringLiteral("Dibujar la siguiente hoja")},
                      krita && m_pages.last().state == SheetState::Draft ? QStringLiteral("seguir en Krita")
                                                                          : QStringLiteral("dibujar la hoja %1").arg(m_pages.last().sheet)});
    if (krita && page.state == SheetState::Draft) // HU-142: el borrador de Krita se termina acá
        hints.append({{QStringLiteral("Terminar la hoja")}, QStringLiteral("terminar la hoja %1").arg(page.sheet)});
    hints.append({{QStringLiteral("Mostrar en el TV")}, tvOn ? QStringLiteral("cortar el TV") : QStringLiteral("mostrar en el TV")});
    hints.append({{QStringLiteral("Volver a la biblioteca")}, QStringLiteral("biblioteca")});
    double ky = height() - 28 - hints.size() * 38 + 10;
    for (const Hint& hint : hints) {
        double x = left;
        for (const QString& name : hint.names)
            x += drawKey(p, x, ky, m_shortcuts.keyOf(name)) + 8;
        p.setFont(themeFont(false, 14));
        p.setPen(theme::kTextoSecundario);
        p.drawText(QRectF(x + 4, ky, std::max(0.0, left + inner - x - 4), 28), Qt::AlignLeft | Qt::AlignVCenter, hint.what);
        ky += 38;
    }
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
