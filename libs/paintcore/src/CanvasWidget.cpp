#include "paintcore/CanvasWidget.h"

#include "Brush.h"

#include <QPainter>
#include <QPaintEvent>

namespace paintcore {

struct CanvasWidget::Impl {
    detail::Brush brush;
};

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
    , d(std::make_unique<Impl>())
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

CanvasWidget::~CanvasWidget() = default;

void CanvasWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), Qt::white);
}

} // namespace paintcore
