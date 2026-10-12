#include "exercises/Turntable.h"

#include <appkit/Theme.h>

#include <QFont>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ejercicios {

const QString TurningExercise::kAxis = QStringLiteral("axis");
const QString TurningExercise::kStep = QStringLiteral("step");
const QString TurningExercise::kPerspective = QStringLiteral("perspective");
const QString TurningExercise::kHidden = QStringLiteral("hidden");
const QString TurningExercise::kSize = QStringLiteral("size");

namespace giro {

double dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 place(Vec3 p, Axis axis, double degrees)
{
    const double r = degrees * std::numbers::pi / 180;
    const double c = std::cos(r), s = std::sin(r);
    switch (axis) {
    case Axis::Vertical: // sobre y: el frente va hacia la derecha
        p = {c * p.x + s * p.z, p.y, -s * p.x + c * p.z};
        break;
    case Axis::Horizontal: // sobre x: el frente va hacia arriba
        p = {p.x, c * p.y + s * p.z, -s * p.y + c * p.z};
        break;
    case Axis::Depth: // sobre z: antihorario en la pantalla
        p = {c * p.x - s * p.y, s * p.x + c * p.y, p.z};
        break;
    }
    // La cámara inclinada: lo de arriba se acerca, así se ve la tapa.
    const double e = kElevationDeg * std::numbers::pi / 180;
    const double ce = std::cos(e), se = std::sin(e);
    return {p.x, ce * p.y - se * p.z, se * p.y + ce * p.z};
}

QPointF project(Vec3 placed, double eye)
{
    const double f = eye / (eye - placed.z);
    return {placed.x * f, placed.y * f};
}

bool facesCamera(Vec3 normal, Vec3 point, double eye)
{
    return dot(normal, {-point.x, -point.y, eye - point.z}) > 0;
}

Axis axisFor(int param, QRandomGenerator& rng)
{
    return Axis(param == TurningExercise::kRandomAxis ? int(rng.bounded(3)) : std::clamp(param, 0, 2));
}

Frame frame(const QVariantMap& values, const SafeZone& zone)
{
    Frame f;
    f.sheet = zone.sheet.isValid() ? zone.sheet
                                   : QRectF(zone.center - QPointF(zone.radius, zone.radius),
                                            QSizeF(2 * zone.radius, 2 * zone.radius));
    f.center = f.sheet.center();
    // La cámara, de lejos (casi isométrica) a cerca (fuga fuerte).
    f.radius = values.value(TurningExercise::kSize).toDouble() / 2;
    const double far = 1 - values.value(TurningExercise::kPerspective).toDouble();
    f.eye = f.radius * (2.5 + 30 * far * far * far);
    // Que entre en la hoja con cualquier giro: lo más lejos que proyecta la esfera que la envuelve.
    const double reach = zone.mm(f.radius * f.eye / std::sqrt(f.eye * f.eye - f.radius * f.radius));
    const double room = 0.42 * std::min(f.sheet.width(), f.sheet.height());
    f.scale = zone.mm(1) * std::min(1.0, room / reach);
    return f;
}

Ghost::Ghost(Generated& out, const Frame& frame, const SafeZone& zone, bool hidden)
    : m_out(out)
    , m_frame(frame)
    , m_painter(&out.guides)
    , m_hidden(hidden)
    , m_margin(zone.mm(6))
    , m_fontPx(std::max(10.0, std::round(zone.mm(3.5))))
{
    const QColor ink = appkit::theme::kMarcaHoja;
    QColor ghost = ink;
    ghost.setAlphaF(appkit::theme::kGhostOpacidad);
    QColor dashed = ink;
    dashed.setAlphaF(appkit::theme::kGhostOpacidad * 0.8f);
    const double width = std::max(1.0, zone.mm(0.3));
    m_solid = QPen(ghost, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    m_behind = QPen(dashed, width * 0.66, Qt::CustomDashLine, Qt::FlatCap);
    m_behind.setDashPattern({4, 4});
    m_painter.setRenderHint(QPainter::Antialiasing);
}

Ghost::~Ghost()
{
    m_painter.end();
}

void Ghost::line(QPointF a, QPointF b, bool visible)
{
    const QPointF pa = m_frame.toSheet(a), pb = m_frame.toSheet(b);
    QPainterPath path(pa);
    path.lineTo(pb);
    m_out.ideal.append(path);
    if (visible || m_hidden) {
        m_painter.setPen(visible ? m_solid : m_behind);
        m_painter.drawLine(pa, pb);
    }
}

void Ghost::curve(std::span<const QPointF> points, const std::vector<bool>& visible)
{
    if (points.size() < 2)
        return;
    QPainterPath ideal(m_frame.toSheet(points[0]));
    for (size_t i = 1; i < points.size(); ++i)
        ideal.lineTo(m_frame.toSheet(points[i]));
    m_out.ideal.append(ideal);
    size_t start = 0;
    while (start + 1 < points.size()) {
        const bool v = visible[start];
        size_t end = start + 1;
        while (end + 1 < points.size() && visible[end] == v)
            ++end;
        if (v || m_hidden) {
            QPainterPath run(m_frame.toSheet(points[start]));
            for (size_t i = start + 1; i <= end; ++i)
                run.lineTo(m_frame.toSheet(points[i]));
            m_painter.setPen(v ? m_solid : m_behind);
            m_painter.drawPath(run);
        }
        start = end;
    }
}

void Ghost::angle(int degrees)
{
    QFont font(QString::fromLatin1(appkit::theme::kFuenteMono));
    font.setPixelSize(int(m_fontPx));
    m_painter.setFont(font);
    m_painter.setPen(appkit::theme::kMarcaHoja);
    const QRectF s = m_frame.sheet;
    m_painter.drawText(QRectF(s.left(), s.top(), s.width() - m_margin, s.height() - m_margin),
                       Qt::AlignRight | Qt::AlignBottom, QStringLiteral("%1°").arg(degrees));
}

} // namespace giro

QString TurningExercise::group() const
{
    return QStringLiteral("Volumen");
}

QList<appkit::Param> TurningExercise::turnParams(int defaultAxis, const QString& hiddenLabel)
{
    using Type = appkit::Param::Type;
    return {
        sheetsParam(12), // media vuelta a 15°
        {.key = kAxis, .label = QStringLiteral("Eje (0 vertical · 1 horizontal · 2 profundidad · 3 al azar)"),
         .type = Type::Integer, .minimum = 0, .maximum = kRandomAxis, .step = 1, .defaultValue = defaultAxis},
        {.key = kStep, .label = QStringLiteral("Paso de giro"), .type = Type::Integer, .minimum = 5, .maximum = 45,
         .step = 5, .defaultValue = 15, .suffix = QStringLiteral("°")},
        {.key = kPerspective, .label = QStringLiteral("Perspectiva (0 casi isométrica · 1 fuerte)"),
         .type = Type::Real, .minimum = 0, .maximum = 1, .step = 0.05, .defaultValue = 0.5},
        {.key = kHidden, .label = hiddenLabel, .type = Type::Toggle, .defaultValue = true},
        {.key = kSize, .label = QStringLiteral("Tamaño (diagonal)"), .type = Type::Integer, .minimum = 60,
         .maximum = 180, .step = 10, .defaultValue = 120, .suffix = QStringLiteral(" mm")},
    };
}

} // namespace ejercicios
