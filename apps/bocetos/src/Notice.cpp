#include "Notice.h"

#include "Paint.h"

#include <appkit/Theme.h>

#include <QPainter>
#include <QPainterPath>

namespace theme = appkit::theme;

namespace {
constexpr int kWidth = 520; // como appkit::ConfirmOverlay
}

Notice::Notice(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);
    hide();
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &QWidget::hide);
}

void Notice::busy(const QString& title, const QString& detail)
{
    present(title, detail, true);
}

void Notice::warn(const QString& title, const QString& detail)
{
    present(title, detail, false);
    m_timer.start(6000);
}

void Notice::present(const QString& title, const QString& detail, bool busy)
{
    m_title = title;
    m_detail = detail;
    m_busy = busy;
    m_timer.stop();
    setAttribute(Qt::WA_TransparentForMouseEvents, !busy);
    setGeometry(parentWidget()->rect());
    show();
    raise();
    update();
}

void Notice::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_busy)
        p.fillRect(rect(), QColor(0, 0, 0, 120));
    const QRectF box((width() - kWidth) / 2.0, (height() - 120) / 2.0, kWidth, 120);
    QPainterPath path;
    path.addRoundedRect(box, theme::kRadioPanel, theme::kRadioPanel);
    p.fillPath(path, theme::kPanel);
    p.setPen(QPen(m_busy ? theme::kBorde : theme::kSeleccionTexto, 1));
    p.drawPath(path);
    p.setFont(bocetos::themeFont(false, 18, QFont::DemiBold));
    p.setPen(theme::kTexto);
    p.drawText(box.adjusted(28, 26, -28, -64), Qt::AlignLeft | Qt::AlignVCenter, m_title);
    p.setFont(bocetos::themeFont(false, 14));
    p.setPen(theme::kTextoSecundario);
    p.drawText(box.adjusted(28, 64, -28, -24), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, m_detail);
}
