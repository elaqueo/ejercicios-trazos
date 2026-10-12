#include "appkit/ShortcutsOverlay.h"

#include "appkit/Theme.h"

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <climits>
#include <vector>

namespace appkit {

namespace {

// Medidas de la mesa "Atajos" del canvas de diseño (px; HU-117).
constexpr int kPadX = 32, kPadY = 28;
constexpr int kTitleHeight = 34;
constexpr int kTitleGap = 22;     // entre el título y la raya
constexpr int kBodyTop = 18;      // de la raya a los grupos
constexpr int kColumnGap = 32;
constexpr int kGroupGap = 22;     // entre grupos de una columna
constexpr int kGroupHeight = 22;  // encabezado de grupo, con su margen
constexpr int kRowGap = 4;
constexpr int kKeysWidth = 84;    // lugar mínimo de las teclas
constexpr int kKeysGap = 10;      // de las teclas al nombre
constexpr int kMargin = 24;       // aire alrededor dentro del lugar que hay
const QColor kGesto{0xC9, 0xCE, 0xD4};
const QColor kGestoBorde{0x59, 0x61, 0x6B};

QFont pixelFont(const QFont& base, int px, int weight = QFont::Normal)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(QFont::Weight(weight));
    return f;
}

QFont monoFont(const QFont& base, int px)
{
    QFont f = pixelFont(base, px, QFont::Medium);
    f.setFamilies({QString::fromLatin1(theme::kFuenteMono), QStringLiteral("Consolas")});
    return f;
}

// Un chip con el texto centrado (la fuente ya puesta); devuelve su ancho. Tecla: relleno y
// borde; gesto (HU-117): solo borde punteado, sin relleno.
int drawChip(QPainter& p, int x, int centerY, int height, const QString& text, bool gesture)
{
    const int w = std::max(height, p.fontMetrics().horizontalAdvance(text) + (gesture ? 16 : 14));
    const QRect box(x, centerY - height / 2, w, height);
    QPainterPath path;
    path.addRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioTecla, theme::kRadioTecla);
    if (gesture) {
        p.setPen(QPen(kGestoBorde, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(kGesto);
    } else {
        p.fillPath(path, theme::kPanelAlto);
        p.setPen(theme::kBorde);
        p.drawPath(path);
        p.setPen(theme::kTexto);
    }
    p.drawText(box, Qt::AlignCenter, text);
    return w;
}

} // namespace

ShortcutsOverlay::ShortcutsOverlay(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground); // esquinas redondeadas (HU-117: se veían puntas)
    QFont f = font();
    f.setFamilies({QString::fromLatin1(theme::kFuente), f.family()});
    setFont(f);
    setSheet({}, {});
}

QList<QList<int>> ShortcutsOverlay::balance(const QList<int>& heights, int columns, int gap)
{
    const int n = int(heights.size());
    columns = std::max(1, columns);
    // span(i, j): el alto de los grupos [i, j) en una columna.
    const auto span = [&](int i, int j) {
        int h = 0;
        for (int k = i; k < j; ++k)
            h += heights[k] + (k > i ? gap : 0);
        return h;
    };
    // best[c][i]: el máximo más bajo para los grupos i.. en c columnas; cut[c][i], dónde termina
    // la primera.
    std::vector<std::vector<int>> best(size_t(columns) + 1, std::vector<int>(size_t(n) + 1, 0));
    std::vector<std::vector<int>> cut(size_t(columns) + 1, std::vector<int>(size_t(n) + 1, n));
    for (int i = 0; i <= n; ++i)
        best[1][size_t(i)] = span(i, n);
    for (int c = 2; c <= columns; ++c)
        for (int i = 0; i <= n; ++i) {
            best[size_t(c)][size_t(i)] = best[1][size_t(i)];
            for (int j = i + 1; j < n; ++j) {
                const int m = std::max(span(i, j), best[size_t(c) - 1][size_t(j)]);
                if (m < best[size_t(c)][size_t(i)]) {
                    best[size_t(c)][size_t(i)] = m;
                    cut[size_t(c)][size_t(i)] = j;
                }
            }
        }
    QList<QList<int>> result(columns);
    int from = 0;
    for (int c = columns; c >= 1 && from < n; --c) {
        const int to = c == 1 ? n : cut[size_t(c)][size_t(from)];
        for (int k = from; k < to; ++k)
            result[columns - c].append(k);
        from = to;
    }
    return result;
}

int ShortcutsOverlay::groupHeight(const ShortcutGroup& group, int rowHeight) const
{
    return kGroupHeight + int(group.rows.size()) * (rowHeight + kRowGap) - kRowGap;
}

void ShortcutsOverlay::setSheet(const QString& app, const QList<ShortcutGroup>& groups, const QSize& available)
{
    m_app = app;
    m_groups = groups;
    m_gestures = std::any_of(groups.begin(), groups.end(), [](const ShortcutGroup& g) {
        return std::any_of(g.rows.begin(), g.rows.end(), [](const ShortcutRow& r) { return r.gesture; });
    });
    const int width = available.isValid() ? std::min(kWidth, available.width() - 2 * kMargin) : kWidth;
    const int maxHeight = available.isValid() ? available.height() - 2 * kMargin : INT_MAX;
    // Del alto de la mesa para abajo, el primero que entra (o el mínimo).
    int height = 0;
    for (m_rowHeight = kRowHeight; m_rowHeight >= kMinRowHeight; --m_rowHeight) {
        QList<int> heights;
        for (const ShortcutGroup& g : m_groups)
            heights.append(groupHeight(g, m_rowHeight));
        m_columns = balance(heights, kColumns, kGroupGap);
        int tallest = 0;
        for (const QList<int>& column : m_columns) {
            int h = 0;
            for (int i = 0; i < column.size(); ++i)
                h += heights[column[i]] + (i > 0 ? kGroupGap : 0);
            tallest = std::max(tallest, h);
        }
        height = kPadY + kTitleHeight + kTitleGap + 1 + kBodyTop + tallest + kPadY;
        if (height <= maxHeight || m_rowHeight == kMinRowHeight)
            break;
    }
    resize(width, height);
    update();
}

void ShortcutsOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath panel;
    panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioPanel, theme::kRadioPanel);
    p.fillPath(panel, theme::kPanel);
    p.setPen(theme::kBorde);
    p.drawPath(panel);

    // Título y el nombre de la app; a la derecha, la leyenda de los gestos y cómo se cierra.
    const QRect titleRect(kPadX, kPadY, width() - 2 * kPadX, kTitleHeight);
    const QFont title = pixelFont(font(), 26, QFont::DemiBold);
    p.setFont(title);
    p.setPen(theme::kTexto);
    const QString heading = QStringLiteral("Atajos");
    p.drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, heading);
    const int appX = titleRect.left() + QFontMetrics(title).horizontalAdvance(heading) + 12;
    p.setFont(pixelFont(font(), 15));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRect(appX, titleRect.top() + 3, titleRect.width(), kTitleHeight), Qt::AlignVCenter | Qt::AlignLeft, m_app);

    const QFont hint = pixelFont(font(), 14);
    const QFont titleMono = monoFont(font(), 13);
    const QFont gestureFont = pixelFont(font(), 13);
    int x = titleRect.right();
    const int titleY = titleRect.center().y();
    const auto text = [&](const QString& s) {
        p.setFont(hint);
        const int w = p.fontMetrics().horizontalAdvance(s);
        x -= w;
        p.setPen(theme::kTextoSecundario);
        p.drawText(QRect(x, titleRect.top(), w, kTitleHeight), Qt::AlignVCenter, s);
        x -= 8;
    };
    const auto chip = [&](const QString& s, bool gesture) {
        p.setFont(gesture ? gestureFont : titleMono);
        const int h = gesture ? 26 : 28;
        const int w = std::max(h, p.fontMetrics().horizontalAdvance(s) + 16);
        x -= w;
        drawChip(p, x, titleY, h, s, gesture);
        x -= 8;
    };
    text(QStringLiteral("cierra"));
    chip(QStringLiteral("Esc"), false);
    text(QStringLiteral("o"));
    chip(QStringLiteral("Ctrl+,"), false);
    if (m_gestures) {
        x -= 16;
        text(QStringLiteral("del lápiz o la tableta"));
        chip(QStringLiteral("gesto"), true);
    }

    // La raya y las columnas.
    const int line = kPadY + kTitleHeight + kTitleGap;
    p.setPen(theme::kBorde);
    p.drawLine(kPadX, line, width() - kPadX, line);
    const int top = line + 1 + kBodyTop;
    const int columnWidth = (width() - 2 * kPadX - (kColumns - 1) * kColumnGap) / kColumns;
    QFont group = pixelFont(font(), 12, QFont::DemiBold);
    group.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    const QFont name = pixelFont(font(), 14);
    const QFont keyFont = monoFont(font(), 12);
    const int chipHeight = std::min(26, m_rowHeight - 2);
    for (int c = 0; c < m_columns.size(); ++c) {
        const int left = kPadX + c * (columnWidth + kColumnGap);
        int y = top;
        for (int index : m_columns[c]) {
            const ShortcutGroup& g = m_groups[index];
            p.setFont(group);
            p.setPen(theme::kTextoSecundario);
            p.drawText(QRect(left, y, columnWidth, kGroupHeight - 4), Qt::AlignLeft | Qt::AlignBottom, g.title.toUpper());
            y += kGroupHeight;
            for (const ShortcutRow& row : g.rows) {
                const int center = y + m_rowHeight / 2;
                p.setFont(row.gesture ? gestureFont : keyFont);
                int kx = left;
                for (const QString& k : row.keys)
                    kx += drawChip(p, kx, center, chipHeight, k, row.gesture) + 4;
                const int nameX = std::max(left + kKeysWidth, kx - 4 + kKeysGap);
                p.setFont(name);
                p.setPen(theme::kTexto);
                const QRect nameRect(nameX, y, left + columnWidth - nameX, m_rowHeight);
                p.drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                           p.fontMetrics().elidedText(row.name, Qt::ElideRight, nameRect.width()));
                y += m_rowHeight + kRowGap;
            }
            y += kGroupGap - kRowGap;
        }
    }
}

void ShortcutsOverlay::closeOverlay()
{
    hide();
    if (onClose)
        onClose();
}

void ShortcutsOverlay::keyPressEvent(QKeyEvent* event)
{
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    if (event->key() == Qt::Key_Escape || (ctrl && event->key() == Qt::Key_Comma))
        closeOverlay();
    else
        QWidget::keyPressEvent(event);
}

} // namespace appkit
