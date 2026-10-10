#include "appkit/MenuOverlay.h"

#include "appkit/Theme.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace appkit {

namespace {

// Medidas de la mesa "Menú overlay" del canvas de diseño (px).
constexpr int kPadding = 32;
constexpr int kGap = 24;          // entre el título, el destacado y la grilla, y entre filas de grupos
constexpr int kColumnGap = 28;
constexpr int kTitleHeight = 34;
constexpr int kFeaturedHeight = 64;
constexpr int kGroupHeight = 22;  // encabezado de grupo
constexpr int kItemGap = 6;

QFont pixelFont(const QFont& base, int px, int weight = QFont::Normal)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(QFont::Weight(weight));
    return f;
}

} // namespace

MenuOverlay::MenuOverlay(QWidget* owner, QString title, Qt::Key closeKey)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
    , m_title(std::move(title))
    , m_closeKey(closeKey)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    QFont f = font();
    f.setFamilies({QString::fromLatin1(theme::kFuente), f.family()});
    setFont(f);
    layoutRows();
}

void MenuOverlay::setGroups(const QList<MenuGroup>& groups)
{
    m_groups = groups;
    layoutRows();
    update();
}

void MenuOverlay::setCurrent(const QString& id)
{
    m_current = id;
    update();
}

QRect MenuOverlay::itemRect(const QString& id) const
{
    for (int row = 0; row < m_rows.size(); ++row)
        if (const MenuItem* item = itemOf(row); item && item->id == id)
            return m_rows[row].rect;
    return {};
}

void MenuOverlay::layoutRows()
{
    // Arriba, a todo el ancho, los grupos sin título (el destacado); después los demás en
    // una grilla de kColumns columnas, fila por fila; cada fila tan alta como su grupo más
    // largo.
    m_rows.clear();
    const int inner = kWidth - 2 * kPadding;
    int y = kPadding + kTitleHeight + kGap;
    QList<int> titled;
    for (int g = 0; g < m_groups.size(); ++g) {
        if (!m_groups[g].title.isEmpty()) {
            titled.append(g);
            continue;
        }
        for (int i = 0; i < m_groups[g].items.size(); ++i) {
            const MenuItem& item = m_groups[g].items[i];
            const int h = item.featured ? kFeaturedHeight : kRowHeight;
            m_rows.append({g, i, QRect(kPadding, y, inner, h)});
            y += h + kItemGap;
        }
        y += kGap - kItemGap;
    }
    const int columnWidth = (inner - (kColumns - 1) * kColumnGap) / kColumns;
    for (int first = 0; first < titled.size(); first += kColumns) {
        int rowBottom = y;
        for (int c = 0; c < kColumns && first + c < titled.size(); ++c) {
            const int g = titled[first + c];
            const int x = kPadding + c * (columnWidth + kColumnGap);
            int gy = y;
            m_rows.append({g, -1, QRect(x, gy, columnWidth, kGroupHeight)});
            gy += kGroupHeight;
            for (int i = 0; i < m_groups[g].items.size(); ++i) {
                m_rows.append({g, i, QRect(x, gy, columnWidth, kRowHeight)});
                gy += kRowHeight + kItemGap;
            }
            rowBottom = std::max(rowBottom, gy - kItemGap);
        }
        y = rowBottom + kGap;
    }
    resize(kWidth, y - kGap + kPadding);
}

const MenuItem* MenuOverlay::itemOf(int row) const
{
    if (row < 0 || row >= m_rows.size() || m_rows[row].item < 0)
        return nullptr;
    return &m_groups[m_rows[row].group].items[m_rows[row].item];
}

int MenuOverlay::rowAt(QPoint point) const
{
    for (int row = 0; row < m_rows.size(); ++row)
        if (m_rows[row].rect.contains(point))
            return row;
    return -1;
}

void MenuOverlay::moveCursor(int step)
{
    // Sin dar la vuelta: en el primero o el último habilitado, el cursor se queda (pasarse y
    // volver sin notarlo al ítem de partida confundía, HU-20).
    const int count = int(m_rows.size());
    int row = m_cursor >= 0 ? m_cursor : (step > 0 ? -1 : count);
    for (row += step; row >= 0 && row < count; row += step) {
        const MenuItem* item = itemOf(row);
        if (item && item->enabled) {
            m_cursor = row;
            update();
            return;
        }
    }
}

void MenuOverlay::moveCursor(int dx, int dy)
{
    // El ítem habilitado más cercano hacia ese lado: lo que está en la misma columna (o
    // fila) primero; sin nada hacia ese lado, el cursor se queda.
    if (m_cursor < 0) {
        moveCursor(dx + dy > 0 ? 1 : -1);
        return;
    }
    const QRect from = m_rows[m_cursor].rect;
    int best = -1;
    double bestScore = 0;
    for (int row = 0; row < m_rows.size(); ++row) {
        const MenuItem* item = itemOf(row);
        if (row == m_cursor || !item || !item->enabled)
            continue;
        const QRect to = m_rows[row].rect;
        double along, across;
        if (dy != 0) {
            along = dy > 0 ? to.top() - from.bottom() : from.top() - to.bottom();
            const bool overlap = to.left() < from.right() && from.left() < to.right();
            across = overlap ? std::abs(to.left() - from.left()) * 0.001 : std::abs(to.center().x() - from.center().x());
        } else {
            along = dx > 0 ? to.left() - from.right() : from.left() - to.right();
            const bool overlap = to.top() < from.bottom() && from.top() < to.bottom();
            across = overlap ? 0 : std::abs(to.center().y() - from.center().y());
        }
        if (along < 0)
            continue; // no está hacia ese lado
        const double score = along + 2 * across;
        if (best < 0 || score < bestScore) {
            best = row;
            bestScore = score;
        }
    }
    if (best >= 0) {
        m_cursor = best;
        update();
    }
}

void MenuOverlay::pick(int row)
{
    const MenuItem* item = itemOf(row);
    if (!item || !item->enabled)
        return;
    m_current = item->id;
    hide();
    if (onPick)
        onPick(item->id);
}

void MenuOverlay::showEvent(QShowEvent* event)
{
    // El cursor del teclado arranca en el actual.
    m_cursor = -1;
    for (int row = 0; row < m_rows.size(); ++row)
        if (const MenuItem* item = itemOf(row); item && item->id == m_current)
            m_cursor = row;
    if (m_cursor < 0)
        moveCursor(1);
    QWidget::showEvent(event);
}

void MenuOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath panel;
    panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioPanel, theme::kRadioPanel);
    p.fillPath(panel, theme::kPanel);
    p.setPen(theme::kBorde);
    p.drawPath(panel);

    // Título y, a la derecha, las teclas que cierran.
    const QRect titleRect(kPadding, kPadding, kWidth - 2 * kPadding, kTitleHeight);
    p.setFont(pixelFont(font(), 26, QFont::DemiBold));
    p.setPen(theme::kTexto);
    p.drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, m_title);
    const QFont hint = pixelFont(font(), 14);
    QFont mono = pixelFont(font(), 13, QFont::Medium);
    mono.setFamilies({QString::fromLatin1(theme::kFuenteMono), QStringLiteral("Consolas")});
    int x = titleRect.right();
    const auto text = [&](const QString& s) {
        p.setFont(hint);
        const int w = p.fontMetrics().horizontalAdvance(s);
        x -= w;
        p.setPen(theme::kTextoSecundario);
        p.drawText(QRect(x, titleRect.top(), w, kTitleHeight), Qt::AlignVCenter, s);
        x -= 8;
    };
    const auto key = [&](const QString& s) {
        p.setFont(mono);
        const int w = std::max(28, p.fontMetrics().horizontalAdvance(s) + 16);
        x -= w;
        const QRect box(x, titleRect.center().y() - 14, w, 28);
        QPainterPath path;
        path.addRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioTecla, theme::kRadioTecla);
        p.fillPath(path, theme::kPanelAlto);
        p.setPen(theme::kBorde);
        p.drawPath(path);
        p.setPen(theme::kTexto);
        p.drawText(box, Qt::AlignCenter, s);
        x -= 8;
    };
    text(QStringLiteral("cierra"));
    key(QStringLiteral("Esc"));
    text(QStringLiteral("o"));
    key(QKeySequence(m_closeKey).toString());

    const QFont group = pixelFont(font(), 12, QFont::DemiBold);
    const QFont normal = pixelFont(font(), 15);
    const QFont strong = pixelFont(font(), 15, QFont::DemiBold);
    const QFont featured = pixelFont(font(), 17, QFont::DemiBold);
    for (int row = 0; row < m_rows.size(); ++row) {
        const Row& r = m_rows[row];
        const MenuItem* item = itemOf(row);
        if (!item) { // encabezado de grupo
            QFont spaced = group;
            spaced.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            p.setFont(spaced);
            p.setPen(theme::kTextoSecundario);
            p.drawText(r.rect.adjusted(0, 0, 0, -6), Qt::AlignLeft | Qt::AlignBottom, m_groups[r.group].title.toUpper());
            continue;
        }
        const QRect box = r.rect;
        const bool current = item->id == m_current;
        const bool marked = item->enabled && (row == m_hover || row == m_cursor);
        QPainterPath path;
        path.addRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioControl, theme::kRadioControl);
        if (current)
            p.fillPath(path, theme::kSeleccion);
        else if (item->featured || marked)
            p.fillPath(path, theme::kPanelAlto);
        if (item->featured && !current) {
            p.setPen(theme::kBorde);
            p.drawPath(path);
        }
        if (row == m_cursor) { // el cursor del teclado, también sobre el actual
            p.setPen(current ? theme::kSeleccionTexto : theme::kTextoSecundario);
            p.drawPath(path);
        }
        const QRect content = box.adjusted(item->featured ? 20 : 12, 0, item->featured ? -20 : -12, 0);
        const QColor color = current ? theme::kSeleccionTexto : item->enabled ? theme::kTexto : theme::kTextoSecundario;
        if (item->featured && !item->description.isEmpty()) {
            p.setFont(featured);
            p.setPen(color);
            p.drawText(content.adjusted(0, 12, 0, 0), Qt::AlignTop | Qt::AlignLeft, item->title);
            p.setFont(hint);
            p.setPen(theme::kTextoSecundario);
            p.drawText(content.adjusted(0, 0, 0, -12), Qt::AlignBottom | Qt::AlignLeft, item->description);
        } else {
            p.setFont(item->featured ? featured : current ? strong : normal);
            p.setPen(color);
            p.drawText(content, Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(item->title, Qt::ElideRight, content.width()));
        }
        if (!item->note.isEmpty()) {
            p.setFont(mono);
            p.setPen(theme::kTextoSecundario);
            p.drawText(content, Qt::AlignVCenter | Qt::AlignRight, item->note);
        }
    }
}

void MenuOverlay::mouseMoveEvent(QMouseEvent* event)
{
    const int row = rowAt(event->position().toPoint());
    if (row != m_hover) {
        m_hover = row;
        update();
    }
}

void MenuOverlay::mousePressEvent(QMouseEvent* event)
{
    pick(rowAt(event->position().toPoint()));
}

void MenuOverlay::leaveEvent(QEvent*)
{
    m_hover = -1;
    update();
}

void MenuOverlay::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();
    if (key == Qt::Key_Up)
        moveCursor(0, -1);
    else if (key == Qt::Key_Down)
        moveCursor(0, 1);
    else if (key == Qt::Key_Left)
        moveCursor(-1, 0);
    else if (key == Qt::Key_Right)
        moveCursor(1, 0);
    else if (key == Qt::Key_Return || key == Qt::Key_Enter)
        pick(m_cursor);
    else if (key == Qt::Key_Escape || key == m_closeKey) {
        hide();
        if (onClose)
            onClose();
    } else
        QWidget::keyPressEvent(event);
}

} // namespace appkit
