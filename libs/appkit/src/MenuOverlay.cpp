#include "appkit/MenuOverlay.h"

#include "appkit/Theme.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QPainterPath>

namespace appkit {

namespace {

constexpr int kPadding = 12;
constexpr int kTitleHeight = 44;
constexpr int kGroupHeight = 32;

} // namespace

MenuOverlay::MenuOverlay(QWidget* owner, QString title, Qt::Key closeKey)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
    , m_title(std::move(title))
    , m_closeKey(closeKey)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
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

void MenuOverlay::layoutRows()
{
    m_rows.clear();
    int y = kTitleHeight;
    for (int g = 0; g < m_groups.size(); ++g) {
        if (!m_groups[g].title.isEmpty()) {
            m_rows.append({g, -1, QRect(kPadding, y, kWidth - 2 * kPadding, kGroupHeight)});
            y += kGroupHeight;
        }
        for (int i = 0; i < m_groups[g].items.size(); ++i) {
            m_rows.append({g, i, QRect(kPadding, y, kWidth - 2 * kPadding, kRowHeight)});
            y += kRowHeight;
        }
    }
    resize(kWidth, y + kPadding);
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
    const int count = int(m_rows.size());
    int row = m_cursor >= 0 ? m_cursor : (step > 0 ? -1 : 0);
    for (int n = 0; n < count; ++n) {
        row = (row + step + count) % count;
        const MenuItem* item = itemOf(row);
        if (item && item->enabled) {
            m_cursor = row;
            update();
            return;
        }
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
    p.fillRect(rect(), theme::kPanel);
    p.setPen(theme::kBorde);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    QFont title = font();
    title.setPointSizeF(title.pointSizeF() * 1.15);
    title.setBold(true);
    p.setFont(title);
    p.setPen(theme::kTexto);
    p.drawText(QRect(kPadding + 4, 0, width(), kTitleHeight), Qt::AlignVCenter, m_title);
    const QFont normal = font();
    p.setFont(normal);
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRect(0, 0, width() - kPadding - 4, kTitleHeight), Qt::AlignVCenter | Qt::AlignRight,
               QKeySequence(m_closeKey).toString() + QStringLiteral(" o Esc cierra"));

    QFont group = font();
    group.setPointSizeF(group.pointSizeF() * 0.9);
    group.setBold(true);
    QFont featured = font();
    featured.setPointSizeF(featured.pointSizeF() * 1.15);
    featured.setBold(true);
    for (int row = 0; row < m_rows.size(); ++row) {
        const Row& r = m_rows[row];
        const MenuItem* item = itemOf(row);
        if (!item) { // encabezado de grupo
            p.setFont(group);
            p.setPen(theme::kTextoSecundario);
            p.drawText(r.rect.adjusted(4, 0, 0, -4), Qt::AlignLeft | Qt::AlignBottom,
                       m_groups[r.group].title.toUpper());
            continue;
        }
        const QRect box = r.rect.adjusted(0, 2, 0, -2);
        const bool current = item->id == m_current;
        const bool marked = item->enabled && (row == m_hover || row == m_cursor);
        if (current || marked || item->featured) {
            QPainterPath path;
            path.addRoundedRect(box, theme::kRadioControl, theme::kRadioControl);
            p.fillPath(path, current ? theme::kSeleccion : theme::kPanelAlto);
            if (row == m_cursor && !current) {
                p.setPen(theme::kTextoSecundario);
                p.drawPath(path);
            }
        }
        p.setFont(item->featured ? featured : normal);
        p.setPen(current ? theme::kSeleccionTexto : item->enabled ? theme::kTexto : theme::kTextoSecundario);
        p.drawText(box.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, item->title);
        if (!item->note.isEmpty()) {
            p.setFont(normal);
            p.setPen(theme::kTextoSecundario);
            p.drawText(box.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignRight, item->note);
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
        moveCursor(-1);
    else if (key == Qt::Key_Down)
        moveCursor(1);
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
