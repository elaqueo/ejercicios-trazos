#include "paintcore/CanvasWidget.h"

#include <QPainter>
#include <QPaintEvent>

namespace paintcore {

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void CanvasWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), Qt::white);
}

} // namespace paintcore
