#include "appkit/ConfirmOverlay.h"

#include "appkit/Theme.h"

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace appkit {

namespace {

// Medidas de la mesa "Confirmación" del canvas de diseño (px).
constexpr int kPadX = 32, kPadY = 28;
constexpr int kTitleHeight = 28;
constexpr int kDetailGap = 6, kDetailHeight = 22;
constexpr int kGap = 20;
constexpr int kKeyHeight = 28;

QFont pixelFont(const QFont& base, int px, int weight = QFont::Normal)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(QFont::Weight(weight));
    return f;
}

} // namespace

ConfirmOverlay::ConfirmOverlay(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::FramelessWindowHint)
{
    setFocusPolicy(Qt::StrongFocus);
    QFont f = font();
    f.setFamilies({QString::fromLatin1(theme::kFuente), f.family()});
    setFont(f);
    resize(kWidth, kPadY + kTitleHeight + kDetailGap + kDetailHeight + kGap + kKeyHeight + kPadY);
}

void ConfirmOverlay::ask(const QString& title, const QString& detail, Qt::Key key, const QString& keyName,
                         const QString& action, Qt::KeyboardModifiers modifiers)
{
    m_modifiers = modifiers;
    m_title = title;
    m_detail = detail;
    m_key = key;
    m_keyName = keyName;
    m_action = action;
    update();
}

void ConfirmOverlay::finish(bool confirmed)
{
    if (!isVisible() || m_finishing)
        return;
    m_finishing = true;
    hide();
    m_finishing = false;
    if (confirmed && onConfirm)
        onConfirm();
    else if (!confirmed && onCancel)
        onCancel();
}

void ConfirmOverlay::keyPressEvent(QKeyEvent* event)
{
    // Solo las teclas de verdad: los modificadores solos (Ctrl, Shift) no deciden nada.
    const int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta)
        return;
    finish(key == m_key && (event->modifiers() & ~Qt::KeypadModifier) == m_modifiers);
}

void ConfirmOverlay::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    finish(false); // un clic en otro lado también cancela
}

void ConfirmOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath panel;
    panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioPanel, theme::kRadioPanel);
    p.fillPath(panel, theme::kPanel);
    p.setPen(theme::kBorde);
    p.drawPath(panel);

    const int inner = kWidth - 2 * kPadX;
    int y = kPadY;
    p.setFont(pixelFont(font(), 20, QFont::DemiBold));
    p.setPen(theme::kTexto);
    p.drawText(QRect(kPadX, y, inner, kTitleHeight), Qt::AlignLeft | Qt::AlignVCenter, m_title);
    y += kTitleHeight + kDetailGap;
    const QFont body = pixelFont(font(), 14);
    p.setFont(body);
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRect(kPadX, y, inner, kDetailHeight), Qt::AlignLeft | Qt::AlignVCenter, m_detail);
    y += kDetailHeight + kGap;

    QFont mono = pixelFont(font(), 13, QFont::Medium);
    mono.setFamilies({QString::fromLatin1(theme::kFuenteMono), QStringLiteral("Consolas")});
    int x = kPadX;
    const auto key = [&](const QString& name, bool primary) {
        p.setFont(mono);
        const int w = std::max(kKeyHeight, p.fontMetrics().horizontalAdvance(name) + 16);
        const QRect box(x, y, w, kKeyHeight);
        QPainterPath path;
        path.addRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioTecla, theme::kRadioTecla);
        p.fillPath(path, primary ? theme::kSeleccion : theme::kPanelAlto);
        p.setPen(primary ? theme::kSeleccionTexto : theme::kBorde);
        p.drawPath(path);
        p.setPen(primary ? theme::kSeleccionTexto : theme::kTexto);
        p.drawText(box, Qt::AlignCenter, name);
        x += w + 8;
    };
    const auto text = [&](const QString& s, const QColor& color) {
        p.setFont(body);
        const int w = p.fontMetrics().horizontalAdvance(s);
        p.setPen(color);
        p.drawText(QRect(x, y, w, kKeyHeight), Qt::AlignVCenter, s);
        x += w + 8;
    };
    key(m_keyName, true);
    text(m_action, theme::kTexto);
    text(QStringLiteral("·"), theme::kTextoSecundario);
    key(QStringLiteral("Esc"), false);
    text(QStringLiteral("o cualquier otra tecla cancela"), theme::kTextoSecundario);
}

} // namespace appkit
