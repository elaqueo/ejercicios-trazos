#include "paintcore/CanvasWidget.h"

#include "Brush.h"
#include "Surface.h"
#include "paintcore/ViewTransform.h"

#include <mypaint-brush.h>

#include <QElapsedTimer>
#include <QInputDevice>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTabletEvent>
#include <QtMath>

#include <optional>

Q_LOGGING_CATEGORY(lcCanvas, "paintcore.canvas")
// Cada muestra de input, para diagnosticar problemas de trazo. Apagado por defecto;
// se activa con QT_LOGGING_RULES="paintcore.input.debug=true".
Q_LOGGING_CATEGORY(lcInput, "paintcore.input", QtWarningMsg)

namespace paintcore {

namespace {

// Qt da la inclinación en grados (-60..60); libmypaint la espera en -1..1.
constexpr float kMaxTiltDegrees = 60.0f;

// dtime (s) de la muestra sin presión que ubica el pincel al empezar un trazo. Con
// esto el suavizado de libmypaint (slow_tracking, hasta 10) llega a 1 - e^-100.
constexpr double kStrokeStartDtime = 10.0;

// dtime (ms) que recibe una muestra cuyo timestamp no avanzó respecto de la anterior.
// Windows Ink entrega timestamps con resolución de ~15,6 ms (el reloj del sistema),
// así que llegan varias muestras seguidas con el mismo valor, y en algún caso podría
// llegar uno menor. Sin esto, un timestamp menor daba un dtime gigante (resta sin
// signo) y libmypaint reseteaba el pincel en mitad del trazo; y los repetidos daban
// dtime ≈ 0, que concentra en una muestra el tiempo de dos.
constexpr double kMinSampleDtimeMs = 4.0;

// Lo que se ve fuera del lienzo cuando la vista está rotada.
const QColor kOutsideColor(0x3a, 0x3a, 0x3a);

// Ángulo (grados, horario) de p alrededor de center.
double pointerAngle(QPointF p, QPointF center)
{
    return qRadiansToDegrees(std::atan2(p.y() - center.y(), p.x() - center.x()));
}

} // namespace

// Una muestra de input, en coordenadas de pantalla (del widget).
struct CanvasWidget::Sample {
    QPointF viewPos;
    float pressure = 0.0f;
    float xTilt = 0.0f;
    float yTilt = 0.0f;
    quint64 timestampMs = 0;
    Qt::KeyboardModifiers modifiers;
};

struct CanvasWidget::Impl {
    detail::Brush brush;
    std::optional<detail::Surface> surface;
    std::optional<BrushPreset> pendingBrush; // se aplica al empezar el próximo trazo
    bool stroking = false;
    quint64 lastTimestampMs = 0; // timestamp crudo de la última muestra (para el log)
    double strokeClockMs = 0.0;  // tiempo del trazo, con las muestras repetidas repartidas

    ViewTransform view;
    bool rotationSnap = true;
    bool rotating = false;
    double rotateStartViewAngle = 0.0;
    double rotateStartPointerAngle = 0.0;

    // Medición del costo de repintar mientras se rota (RNF-02).
    int rotationFrames = 0;
    qint64 rotationTotalNs = 0;
    qint64 rotationMaxNs = 0;

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
                                  0.0f, 0.0f, 0.0f, kStrokeStartDtime, 1.0f, viewRotationRadians(), 0.0f);
        surface->endAtomic();

        stroking = true;
        lastTimestampMs = timestampMs;
        strokeClockMs = double(timestampMs);
    }

    // Manda una muestra (en coordenadas del lienzo) a libmypaint y devuelve el
    // rectángulo del lienzo que cambió.
    QRect strokeTo(QPointF pos, float pressure, float xTilt, float yTilt, quint64 timestampMs)
    {
        // Reloj del trazo: si el timestamp avanzó, se usa el tiempo real transcurrido
        // desde el reloj; si no (timestamp repetido o fuera de orden), la muestra recibe
        // kMinSampleDtimeMs y el reloj se adelanta, así la próxima muestra con
        // timestamp nuevo descuenta ese tiempo y el total del trazo no cambia.
        double dtimeMs = double(timestampMs) - strokeClockMs;
        if (dtimeMs > 0.0) {
            strokeClockMs = double(timestampMs);
        } else {
            dtimeMs = kMinSampleDtimeMs;
            strokeClockMs += kMinSampleDtimeMs;
        }
        lastTimestampMs = timestampMs;
        const double dtime = dtimeMs / 1000.0;

        // La inclinación llega en ejes de pantalla; libmypaint la refiere al lienzo
        // con viewrotation (igual que la dirección del trazo y el ángulo del dab).
        surface->beginAtomic();
        mypaint_brush_stroke_to_2(brush.handle(), surface->handle(),
                                  float(pos.x()), float(pos.y()), pressure,
                                  xTilt / kMaxTiltDegrees, yTilt / kMaxTiltDegrees, dtime,
                                  1.0f /*viewzoom*/, viewRotationRadians(), 0.0f /*barrel_rotation*/);
        return surface->endAtomic();
    }

    float viewRotationRadians() const { return float(qDegreesToRadians(view.angle())); }
};

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
    , d(std::make_unique<Impl>())
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus); // para la tecla de reset de la rotación
}

CanvasWidget::~CanvasWidget() = default;

void CanvasWidget::setBrush(const BrushPreset& preset)
{
    if (d->stroking)
        d->pendingBrush = preset;
    else
        d->applyBrush(preset);
}

void CanvasWidget::clear()
{
    if (d->surface)
        d->surface->clear();
    update();
}

double CanvasWidget::viewRotation() const
{
    return d->view.angle();
}

void CanvasWidget::setViewRotation(double degrees)
{
    const double angle = ViewTransform::normalized(degrees);
    if (qFuzzyCompare(angle + 1.0, d->view.angle() + 1.0))
        return;
    d->view.setAngle(angle);
    update();
    emit viewRotationChanged(angle);
}

bool CanvasWidget::rotationSnap() const
{
    return d->rotationSnap;
}

void CanvasWidget::setRotationSnap(bool enabled)
{
    d->rotationSnap = enabled;
}

void CanvasWidget::paintEvent(QPaintEvent* event)
{
    QElapsedTimer timer;
    timer.start();

    QPainter painter(this);
    if (d->view.angle() == 0.0) {
        // Sin rotación: copia directa, sin transformar.
        painter.fillRect(event->rect(), Qt::white);
        if (d->surface)
            painter.drawImage(event->rect(), d->surface->image(), event->rect());
    } else {
        painter.fillRect(event->rect(), kOutsideColor);
        // Mientras se rota se prioriza la fluidez; en reposo, la calidad.
        painter.setRenderHint(QPainter::SmoothPixmapTransform, !d->rotating);
        painter.setTransform(d->view.matrix());
        painter.fillRect(rect(), Qt::white);
        if (d->surface)
            painter.drawImage(QPointF(0, 0), d->surface->image());
    }

    if (d->rotating) {
        const qint64 ns = timer.nsecsElapsed();
        ++d->rotationFrames;
        d->rotationTotalNs += ns;
        d->rotationMaxNs = qMax(d->rotationMaxNs, ns);
    }
}

void CanvasWidget::resizeEvent(QResizeEvent* event)
{
    // El lienzo es fijo a pantalla completa (HU-09); un cambio de tamaño lo reinicia.
    d->ensureSurface(size());
    d->view.setCenter(QRectF(rect()).center());
    QWidget::resizeEvent(event);
}

void CanvasWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == kResetRotationKey && !event->isAutoRepeat()) {
        resetViewRotation();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CanvasWidget::updateCanvasRect(const QRect& canvasRect)
{
    if (canvasRect.isEmpty())
        return;
    if (d->view.angle() == 0.0)
        update(canvasRect);
    else
        update(d->view.matrix().mapRect(canvasRect).adjusted(-2, -2, 2, 2));
}

void CanvasWidget::handlePointer(Phase phase, const Sample& sample)
{
    d->ensureSurface(size());

    // Gesto de rotación: modificador + arrastre. Rota según el ángulo que recorre
    // la punta alrededor del centro de la vista.
    if (phase == Phase::Press && sample.modifiers.testFlag(kRotateModifier)) {
        d->rotating = true;
        d->rotateStartViewAngle = d->view.angle();
        d->rotateStartPointerAngle = pointerAngle(sample.viewPos, d->view.center());
        d->rotationFrames = 0;
        d->rotationTotalNs = d->rotationMaxNs = 0;
        return;
    }
    if (d->rotating) {
        const double delta = pointerAngle(sample.viewPos, d->view.center()) - d->rotateStartPointerAngle;
        double angle = d->rotateStartViewAngle + delta;
        if (d->rotationSnap)
            angle = ViewTransform::snapped(angle, kRotationSnapStep);
        setViewRotation(angle);
        if (phase == Phase::Release) {
            d->rotating = false;
            update(); // repinta con suavizado
            if (d->rotationFrames > 0)
                qCInfo(lcCanvas).nospace() << "Rotación: " << d->rotationFrames << " cuadros a " << width() << "x"
                                           << height() << ", promedio "
                                           << d->rotationTotalNs / d->rotationFrames / 1e6 << " ms, máximo "
                                           << d->rotationMaxNs / 1e6 << " ms";
        }
        return;
    }

    const QPointF canvasPos = d->view.toCanvas(sample.viewPos);
    qCDebug(lcInput).nospace() << "fase=" << int(phase) << " trazo=" << d->stroking << " pos=" << sample.viewPos.x()
                               << "," << sample.viewPos.y() << " p=" << sample.pressure << " tilt=" << sample.xTilt
                               << "," << sample.yTilt << " ts=" << sample.timestampMs
                               << " dt_ms=" << qint64(sample.timestampMs) - qint64(d->lastTimestampMs);
    switch (phase) {
    case Phase::Press:
        d->beginStroke(canvasPos, sample.timestampMs);
        break;
    case Phase::Move:
        if (!d->stroking)
            return;
        break;
    case Phase::Release:
        if (!d->stroking)
            return;
        d->stroking = false;
        break;
    }
    // Al soltar, presión 0 cierra el trazo en libmypaint.
    const float pressure = phase == Phase::Release ? 0.0f : sample.pressure;
    updateCanvasRect(d->strokeTo(canvasPos, pressure, sample.xTilt, sample.yTilt, sample.timestampMs));
}

void CanvasWidget::tabletEvent(QTabletEvent* event)
{
    // Cada QTabletEvent es una muestra: no se descarta ni se agrupa ninguna (RNF-04).
    // Aceptarlo evita que Qt sintetice además un evento de mouse.
    event->accept();

    Phase phase;
    switch (event->type()) {
    case QEvent::TabletPress: phase = Phase::Press; break;
    case QEvent::TabletMove: phase = Phase::Move; break;
    case QEvent::TabletRelease: phase = Phase::Release; break;
    default: return;
    }
    handlePointer(phase, {event->position(), float(event->pressure()), float(event->xTilt()),
                          float(event->yTilt()), event->timestamp(), event->modifiers()});
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
    handlePointer(Phase::Press, {event->position(), kMousePressure, 0, 0, event->timestamp(), event->modifiers()});
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!isRealMouse(event) || !(event->buttons() & Qt::LeftButton))
        return;
    handlePointer(Phase::Move, {event->position(), kMousePressure, 0, 0, event->timestamp(), event->modifiers()});
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isRealMouse(event) || event->button() != Qt::LeftButton)
        return;
    handlePointer(Phase::Release, {event->position(), 0.0f, 0, 0, event->timestamp(), event->modifiers()});
}

} // namespace paintcore
