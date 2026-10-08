#include "CalibrationOverlay.h"

#include "appkit/Theme.h"
#include "appkit/UsableArea.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTabletEvent>

namespace appkit::detail {

CalibrationOverlay::CalibrationOverlay(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    hide();
}

void CalibrationOverlay::start()
{
    m_corners.clear();
    setGeometry(parentWidget()->rect());
    show();
    raise();
    setFocus();
    update();
}

void CalibrationOverlay::addCorner(QPointF pos)
{
    m_corners.append(pos);
    update();
    if (m_corners.size() == 2) {
        hide();
        emit finished(rectFromCorners(m_corners[0], m_corners[1]));
    }
}

void CalibrationOverlay::tabletEvent(QTabletEvent* event)
{
    event->accept(); // que no se sintetice un clic de mouse además
    if (event->type() == QEvent::TabletPress)
        addCorner(event->position());
}

void CalibrationOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        addCorner(event->position());
}

void CalibrationOverlay::cancel()
{
    hide();
    emit cancelled();
}

void CalibrationOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    QColor fondo = theme::kFondo;
    fondo.setAlpha(235);
    painter.fillRect(rect(), fondo);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool first = m_corners.isEmpty();
    const QString text = first
        ? tr("Calibrar el área útil (1 de 2)\n\nTocá con el lápiz la esquina SUPERIOR IZQUIERDA\n"
             "de la superficie activa de la tableta.\n\nEsc cancela.")
        : tr("Calibrar el área útil (2 de 2)\n\nAhora tocá la esquina INFERIOR DERECHA.\n\nEsc cancela.");
    QFont font = painter.font();
    font.setPointSizeF(font.pointSizeF() * 1.6);
    painter.setFont(font);
    painter.setPen(theme::kTexto);
    painter.drawText(rect(), Qt::AlignCenter, text);

    // Flecha hacia la esquina que se pide.
    const QPointF corner = first ? QPointF(24, 24) : QPointF(width() - 24, height() - 24);
    const QPointF tail = first ? corner + QPointF(90, 90) : corner - QPointF(90, 90);
    painter.setPen(QPen(theme::kEnfasis, 6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(tail, corner);
    painter.drawEllipse(corner, 10, 10);

    // La primera esquina ya marcada.
    if (!first) {
        painter.setPen(QPen(theme::kGuiaSuave, 3));
        painter.drawEllipse(m_corners[0], 12, 12);
    }
}

} // namespace appkit::detail
