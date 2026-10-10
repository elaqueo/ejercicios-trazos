#include "appkit/ShortcutsOverlay.h"

#include "appkit/Theme.h"

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace appkit {

namespace {

// Medidas de la mesa "Atajos" del canvas de diseño (px).
constexpr int kPadding = 32;
constexpr int kGap = 24;          // entre el título, las columnas y la nota; entre grupos
constexpr int kColumnGap = 28;
constexpr int kTitleHeight = 34;
constexpr int kGroupHeight = 24;  // encabezado de grupo, con su margen
constexpr int kRowGap = 4;
constexpr int kKeysWidth = 112;   // columna de las teclas
constexpr int kNoteHeight = 34;   // separador y nota al pie
const QColor kGesto{0xC9, 0xCE, 0xD4};

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

// Un chip de tecla con el texto centrado; devuelve su ancho.
int drawKey(QPainter& p, int x, int centerY, int height, const QString& text)
{
    const int w = std::max(height, p.fontMetrics().horizontalAdvance(text) + 14);
    const QRect box(x, centerY - height / 2, w, height);
    QPainterPath path;
    path.addRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioTecla, theme::kRadioTecla);
    p.fillPath(path, theme::kPanelAlto);
    p.setPen(theme::kBorde);
    p.drawPath(path);
    p.setPen(theme::kTexto);
    p.drawText(box, Qt::AlignCenter, text);
    return w;
}

} // namespace

ShortcutsOverlay::ShortcutsOverlay(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
{
    setFocusPolicy(Qt::StrongFocus);
    QFont f = font();
    f.setFamilies({QString::fromLatin1(theme::kFuente), f.family()});
    setFont(f);
    setSheet({}, {});
}

int ShortcutsOverlay::groupHeight(const ShortcutGroup& group) const
{
    return kGroupHeight + int(group.rows.size()) * (kRowHeight + kRowGap) - kRowGap;
}

void ShortcutsOverlay::setSheet(const QString& app, const QList<ShortcutGroup>& groups)
{
    m_app = app;
    m_groups = groups;
    m_columns = QList<QList<int>>(kColumns);
    for (int i = 0; i < m_groups.size(); ++i)
        m_columns[std::clamp(m_groups[i].column, 0, kColumns - 1)].append(i);
    int tallest = 0;
    for (const QList<int>& column : m_columns) {
        int h = 0;
        for (int i = 0; i < column.size(); ++i)
            h += groupHeight(m_groups[column[i]]) + (i > 0 ? kGap : 0);
        tallest = std::max(tallest, h);
    }
    resize(kWidth, kPadding + kTitleHeight + kGap + tallest + kGap + kNoteHeight + kPadding);
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

    // Título, el nombre de la app al lado y, a la derecha, las teclas que cierran.
    const QRect titleRect(kPadding, kPadding, kWidth - 2 * kPadding, kTitleHeight);
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
        p.setFont(titleMono);
        const int w = std::max(28, p.fontMetrics().horizontalAdvance(s) + 16);
        x -= w;
        drawKey(p, x, titleRect.center().y(), 28, s);
        x -= 8;
    };
    text(QStringLiteral("cierra"));
    key(QStringLiteral("Esc"));
    text(QStringLiteral("o"));
    key(QStringLiteral("Ctrl+,"));

    // Columnas.
    const int top = kPadding + kTitleHeight + kGap;
    const int columnWidth = (kWidth - 2 * kPadding - (kColumns - 1) * kColumnGap) / kColumns;
    QFont group = pixelFont(font(), 12, QFont::DemiBold);
    group.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    const QFont name = pixelFont(font(), 14);
    const QFont keyFont = monoFont(font(), 12);
    for (int c = 0; c < m_columns.size(); ++c) {
        const int left = kPadding + c * (columnWidth + kColumnGap);
        int y = top;
        for (int index : m_columns[c]) {
            const ShortcutGroup& g = m_groups[index];
            p.setFont(group);
            p.setPen(theme::kTextoSecundario);
            p.drawText(QRect(left, y, columnWidth, kGroupHeight - 6), Qt::AlignLeft | Qt::AlignBottom, g.title.toUpper());
            y += kGroupHeight;
            for (const ShortcutRow& row : g.rows) {
                const int center = y + kRowHeight / 2;
                p.setFont(keyFont);
                int kx = left;
                for (const QString& k : row.keys)
                    kx += drawKey(p, kx, center, 26, k) + 4;
                const int nameX = std::max(left + kKeysWidth, kx + 8);
                p.setFont(name);
                p.setPen(row.gesture ? kGesto : theme::kTexto);
                const QRect nameRect(nameX, y, left + columnWidth - nameX, kRowHeight);
                p.drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                           p.fontMetrics().elidedText(row.name, Qt::ElideRight, nameRect.width()));
                y += kRowHeight + kRowGap;
            }
            y += kGap - kRowGap;
        }
    }

    // Nota al pie.
    const int noteTop = height() - kPadding - kNoteHeight;
    p.setPen(theme::kBorde);
    p.drawLine(kPadding, noteTop, kWidth - kPadding, noteTop);
    p.setFont(pixelFont(font(), 13));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRect(kPadding, noteTop, kWidth - 2 * kPadding, kNoteHeight), Qt::AlignLeft | Qt::AlignBottom,
               QStringLiteral("Los gestos del lápiz y de la tableta van en gris claro: no son teclas, pero también están."));
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
