#include "paintcore/CanvasWidget.h"

#include "Brush.h"
#include "Surface.h"

#include <mypaint-brush.h>

#include <QInputDevice>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTabletEvent>

#include <optional>

Q_LOGGING_CATEGORY(lcCanvas, "paintcore.canvas")

namespace paintcore {

namespace {

// Qt da la inclinación en grados (-60..60); libmypaint la espera en -1..1.
constexpr float kMaxTiltDegrees = 60.0f;

// dtime (s) de la muestra sin presión que ubica el pincel al empezar un trazo. Con
// esto el suavizado de libmypaint (slow_tracking, hasta 10) llega a 1 - e^-100.
constexpr double kStrokeStartDtime = 10.0;

} // namespace

struct CanvasWidget::Impl {
    detail::Brush brush;
    std::optional<detail::Surface> surface;
    std::optional<BrushPreset> pendingBrush; // se aplica al empezar el próximo trazo
    bool stroking = false;
    quint64 lastTimestampMs = 0;

    void applyBrush(const BrushPreset& preset)
    {
        if (preset.isDefault())
            brush.loadDefault();
        else if (!brush.loadJson(preset.json))
            qCWarning(lcCanvas) << "No se pudo cargar el pincel" << preset.name << "; se usa el pincel por defecto";
    }

    void ensureSurface(QSize size)
    {
        if (!surface || surface->size() != size)
            surface.emplace(size);
    }

    void beginStroke(QPointF pos, quint64 timestampMs)
    {
        if (pendingBrush) {
            applyBrush(*pendingBrush);
            pendingBrush.reset();
        }
        mypaint_brush_reset(brush.handle());
        mypaint_brush_new_stroke(brush.handle());

        // Ubica el pincel en el punto de apoyo sin pintar. libmypaint aplica el
        // suavizado (slow_tracking) ANTES del reset, con el dtime de la muestra: con
        // dtime ≈ 0 la posición reseteada quedaría en el final del trazo anterior y
        // el trazo nuevo arrancaría con una línea desde ahí. Con un dtime grande el
        // suavizado salta al punto nuevo, y el reset vuelve sin pintar.
        surface->beginAtomic();
        mypaint_brush_stroke_to_2(brush.handle(), surface->handle(), float(pos.x()), float(pos.y()),
                                  0.0f, 0.0f, 0.0f, kStrokeStartDtime, 1.0f, 0.0f, 0.0f);
        surface->endAtomic();

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

void CanvasWidget::setBrush(const BrushPreset& preset)
{
    if (d->stroking)
        d->pendingBrush = preset;
    else
        d->applyBrush(preset);
}

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
        d->beginStroke(event->position(), event->timestamp());
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
    d->beginStroke(event->position(), event->timestamp());
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
