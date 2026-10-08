#include "paintcore/CanvasWidget.h"

#include "Brush.h"
#include "Surface.h"

#include <mypaint-brush.h>

#include <QInputDevice>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTabletEvent>

#include <optional>

namespace paintcore {

namespace {

// Qt da la inclinación en grados (-60..60); libmypaint la espera en -1..1.
constexpr float kMaxTiltDegrees = 60.0f;

} // namespace

struct CanvasWidget::Impl {
    detail::Brush brush;
    std::optional<detail::Surface> surface;
    bool stroking = false;
    quint64 lastTimestampMs = 0;

    void ensureSurface(QSize size)
    {
        if (!surface || surface->size() != size)
            surface.emplace(size);
    }

    void beginStroke(quint64 timestampMs)
    {
        mypaint_brush_reset(brush.handle());
        mypaint_brush_new_stroke(brush.handle());
        stroking = true;
        lastTimestampMs = timestampMs;
    }

    // Manda una muestra a libmypaint y devuelve el rectángulo a repintar.
    QRect strokeTo(QPointF pos, float pressure, float xTilt, float yTilt, quint64 timestampMs)
    {
        // libmypaint protege dtime <= 0 (eventos con el mismo milisegundo).
        const double dtime = (timestampMs - lastTimestampMs) / 1000.0;
        lastTimestampMs = timestampMs;

        surface->beginAtomic();
        mypaint_brush_stroke_to_2(brush.handle(), surface->handle(),
                                  float(pos.x()), float(pos.y()), pressure,
                                  xTilt / kMaxTiltDegrees, yTilt / kMaxTiltDegrees, dtime,
                                  1.0f /*viewzoom*/, 0.0f /*viewrotation*/, 0.0f /*barrel_rotation*/);
        return surface->endAtomic();
    }
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
    if (d->surface)
        painter.drawImage(event->rect(), d->surface->image(), event->rect());
}

void CanvasWidget::resizeEvent(QResizeEvent* event)
{
    // El lienzo es fijo a pantalla completa (HU-09); un cambio de tamaño lo reinicia.
    d->ensureSurface(size());
    QWidget::resizeEvent(event);
}

void CanvasWidget::tabletEvent(QTabletEvent* event)
{
    // Cada QTabletEvent es una muestra: no se descarta ni se agrupa ninguna (RNF-04).
    // Aceptarlo evita que Qt sintetice además un evento de mouse.
    event->accept();
    d->ensureSurface(size());

    switch (event->type()) {
    case QEvent::TabletPress:
        d->beginStroke(event->timestamp());
        break;
    case QEvent::TabletMove:
        if (!d->stroking)
            return;
        break;
    case QEvent::TabletRelease:
        if (!d->stroking)
            return;
        d->stroking = false;
        break;
    default:
        return;
    }

    // Al soltar, presión 0 cierra el trazo en libmypaint.
    const float pressure = event->type() == QEvent::TabletRelease ? 0.0f : float(event->pressure());
    update(d->strokeTo(event->position(), pressure, event->xTilt(), event->yTilt(), event->timestamp()));
}

namespace {

// Windows también genera eventos de mouse a partir del lápiz; esos se ignoran
// para no pintar dos veces.
bool isRealMouse(const QMouseEvent* event)
{
    return event->device()->type() == QInputDevice::DeviceType::Mouse;
}

} // namespace

void CanvasWidget::mousePressEvent(QMouseEvent* event)
{
    if (!isRealMouse(event) || event->button() != Qt::LeftButton)
        return;
    d->ensureSurface(size());
    d->beginStroke(event->timestamp());
    update(d->strokeTo(event->position(), kMousePressure, 0.0f, 0.0f, event->timestamp()));
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!isRealMouse(event) || !d->stroking)
        return;
    update(d->strokeTo(event->position(), kMousePressure, 0.0f, 0.0f, event->timestamp()));
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isRealMouse(event) || event->button() != Qt::LeftButton || !d->stroking)
        return;
    d->stroking = false;
    update(d->strokeTo(event->position(), 0.0f, 0.0f, 0.0f, event->timestamp()));
}

} // namespace paintcore
