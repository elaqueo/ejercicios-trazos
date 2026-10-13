#include "TvChooser.h"

#include "Paint.h"

#include <appkit/Theme.h>

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace theme = appkit::theme;
using bocetos::drawKey;
using bocetos::themeFont;

namespace {
constexpr double kWidth = 520, kRow = 48, kTop = 96; // del cartel al primer TV
}

TvChooser::TvChooser(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    hide();
}

void TvChooser::choose(const QList<cast::Renderer>& renderers)
{
    m_renderers = renderers;
    m_selected = 0;
    setGeometry(parentWidget()->rect());
    show();
    raise();
    setFocus();
    update();
}

QRectF TvChooser::box() const
{
    const double h = kTop + m_renderers.size() * (kRow + 4) + 64;
    return QRectF((width() - kWidth) / 2, (height() - h) / 2, kWidth, h);
}

QRectF TvChooser::row(int i) const
{
    const QRectF b = box();
    return QRectF(b.left() + 28, b.top() + kTop + i * (kRow + 4), kWidth - 56, kRow);
}

void TvChooser::finish(bool picked)
{
    hide();
    if (picked && onPick)
        onPick(m_renderers.value(m_selected));
    else if (!picked && onCancel)
        onCancel();
}

void TvChooser::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        m_selected = std::max(0, m_selected - 1);
        update();
        break;
    case Qt::Key_Down:
        m_selected = std::min(int(m_renderers.size()) - 1, m_selected + 1);
        update();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        finish(true);
        break;
    case Qt::Key_Escape:
        finish(false);
        break;
    default:
        break;
    }
}

void TvChooser::mousePressEvent(QMouseEvent* event)
{
    for (int i = 0; i < m_renderers.size(); ++i)
        if (row(i).contains(event->position())) {
            m_selected = i;
            finish(true);
            return;
        }
}

void TvChooser::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0, 0, 0, 120));
    const QRectF b = box();
    QPainterPath panel;
    panel.addRoundedRect(b, theme::kRadioPanel, theme::kRadioPanel);
    p.fillPath(panel, theme::kPanel);
    p.setPen(QPen(theme::kBorde, 1));
    p.drawPath(panel);
    p.setFont(themeFont(false, 18, QFont::DemiBold));
    p.setPen(theme::kTexto);
    p.drawText(QRectF(b.left() + 28, b.top() + 24, kWidth - 56, 26), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Mostrar en el TV"));
    p.setFont(themeFont(false, 14));
    p.setPen(theme::kTextoSecundario);
    p.drawText(QRectF(b.left() + 28, b.top() + 52, kWidth - 56, 22), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Hay %1 TVs en la red. El elegido queda para la próxima vez.").arg(m_renderers.size()));
    for (int i = 0; i < m_renderers.size(); ++i) {
        const QRectF r = row(i);
        const bool sel = i == m_selected;
        if (sel) {
            QPainterPath path;
            path.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), theme::kRadioControl, theme::kRadioControl);
            p.fillPath(path, theme::kSeleccion);
            p.setPen(QPen(theme::kSeleccionTexto, 1));
            p.drawPath(path);
        }
        const QColor color = sel ? theme::kSeleccionTexto : theme::kTexto;
        // El TV: una pantalla y su pie.
        p.setPen(QPen(color, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(r.left() + 14, r.center().y() - 8, 20, 12), 2, 2);
        p.drawLine(QPointF(r.left() + 20, r.center().y() + 7), QPointF(r.left() + 28, r.center().y() + 7));
        p.setFont(themeFont(true, 13));
        p.setPen(theme::kTextoSecundario);
        const QString host = m_renderers[i].host();
        const double hostW = QFontMetricsF(p.font()).horizontalAdvance(host);
        p.drawText(QRectF(r.right() - 14 - hostW, r.top(), hostW + 2, r.height()), Qt::AlignLeft | Qt::AlignVCenter, host);
        p.setFont(themeFont(false, 16));
        p.setPen(color);
        const double nameW = r.width() - 48 - hostW - 28;
        p.drawText(QRectF(r.left() + 46, r.top(), nameW, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetricsF(p.font()).elidedText(m_renderers[i].name, Qt::ElideRight, nameW));
    }
    double x = b.left() + 28;
    const double y = b.bottom() - 24 - 28;
    const auto hint = [&](const QStringList& keys, const QString& text, bool strong) {
        for (const QString& k : keys)
            x += drawKey(p, x, y, k) + 6;
        p.setFont(themeFont(false, 14));
        p.setPen(strong ? theme::kTexto : theme::kTextoSecundario);
        const double w = QFontMetricsF(p.font()).horizontalAdvance(text);
        p.drawText(QRectF(x + 2, y, w + 2, 28), Qt::AlignLeft | Qt::AlignVCenter, text);
        x += w + 22;
    };
    hint({QStringLiteral("↑"), QStringLiteral("↓")}, QStringLiteral("elegir"), false);
    hint({QStringLiteral("Enter")}, QStringLiteral("mostrar"), true);
    hint({QStringLiteral("Esc")}, QStringLiteral("cancelar"), false);
}
