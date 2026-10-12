#include "exercises/Elipse.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QFont>
#include <QPainter>
#include <QRandomGenerator>
#include <QTransform>

#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kSizeMin = QStringLiteral("sizeMin");
const QString kSizeMax = QStringLiteral("sizeMax");

constexpr qreal kLabelGap = 34; // del extremo del eje menor al centro del rótulo

// Rótulo del grado en ámbar, centrado en `at`.
void degreeLabel(QPainter& painter, QPointF at, int degrees)
{
    painter.save();
    QFont font(QString::fromLatin1(appkit::theme::kFuenteMono));
    font.setPixelSize(20);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(appkit::theme::kEnfasis);
    painter.drawText(QRectF(at.x() - 40, at.y() - 14, 80, 28), Qt::AlignCenter, QStringLiteral("%1°").arg(degrees));
    painter.restore();
}

} // namespace

QString Elipse::degreeKey(int degrees)
{
    return QStringLiteral("deg%1").arg(degrees);
}

QString Elipse::id() const
{
    return QStringLiteral("elipse");
}

QString Elipse::title() const
{
    return QStringLiteral("Elipses por grado");
}

QString Elipse::group() const
{
    return QStringLiteral("Elipses");
}

QList<appkit::Param> Elipse::params() const
{
    using Type = appkit::Param::Type;
    QList<appkit::Param> list;
    for (const int degrees : kDegrees)
        list.append({.key = degreeKey(degrees), .label = QStringLiteral("%1°").arg(degrees), .type = Type::Toggle,
                     .defaultValue = true});
    list.append({.key = kSizeMin, .label = QStringLiteral("Eje mayor mínimo (× diámetro de la zona)"),
                 .type = Type::Real, .minimum = 0.1, .maximum = 0.8, .step = 0.05, .defaultValue = 0.25});
    list.append({.key = kSizeMax, .label = QStringLiteral("Eje mayor máximo (× diámetro de la zona)"),
                 .type = Type::Real, .minimum = 0.1, .maximum = 0.8, .step = 0.05, .defaultValue = 0.70});
    return list;
}

Generated Elipse::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    QList<int> enabled;
    for (const int degrees : kDegrees)
        if (v.value(degreeKey(degrees)).toBool())
            enabled.append(degrees);
    if (enabled.isEmpty()) // sin ninguno, valen todos
        enabled = QList<int>(std::begin(kDegrees), std::end(kDegrees));
    const int degrees = enabled[int(rng.bounded(quint32(enabled.size())))];
    const qreal sizeMin = v.value(kSizeMin).toDouble();
    const qreal sizeMax = qMax(sizeMin, v.value(kSizeMax).toDouble());

    // Semiejes en el marco del ejercicio: el mayor horizontal, el menor vertical (la
    // orientación de la zona los rota).
    qreal a = (sizeMin + rng.generateDouble() * (sizeMax - sizeMin)) * zone.radius;
    qreal b = a * std::sin(degrees * std::numbers::pi / 180);
    // Lo que tiene que entrar en la zona, medido desde el centro: los puntos de control de la
    // elipse (llegan a las esquinas del rectángulo que la encierra), los anillos de paso y el
    // rótulo más allá del eje menor. Si no entra, se achica.
    const auto reach = [&] {
        return std::max({std::hypot(a, b), a + appkit::theme::kRadioPuntoPaso, b + kLabelGap + 16});
    };
    const qreal margin = 4;
    while (reach() > zone.radius - margin) {
        a *= 0.95;
        b *= 0.95;
    }
    const qreal room = std::max(0.0, zone.radius - margin - reach());
    const qreal along = rng.generateDouble() * 2 * std::numbers::pi;
    const QPointF center = QPointF(std::cos(along), std::sin(along)) * room * std::sqrt(rng.generateDouble());
    // El rótulo va de un lado u otro del eje menor.
    const qreal labelSide = rng.bounded(2) ? 1.0 : -1.0;

    const auto toCanvas = [&](QPointF local) { return zone.toCanvas(center + local); };
    const QPointF c = toCanvas({});
    const QPointF major1 = toCanvas({-a, 0}), major2 = toCanvas({a, 0});
    const QPointF minor1 = toCanvas({0, -b}), minor2 = toCanvas({0, b});
    const QPointF axisFrom = toCanvas({0, -(b + 24)}), axisTo = toCanvas({0, b + 24});

    QTransform transform;
    transform.translate(c.x(), c.y());
    transform.rotateRadians(zone.orientation);
    QPainterPath ellipse;
    ellipse.addEllipse(QPointF(), a, b);

    Generated out;
    out.ideal.append(transform.map(ellipse));
    QPainterPath majorAxis(major1);
    majorAxis.lineTo(major2);
    out.ideal.append(majorAxis);
    QPainterPath minorAxis(minor1);
    minorAxis.lineTo(minor2);
    out.ideal.append(minorAxis);

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::constructionLine(painter, axisFrom, axisTo);
    painter.setPen(Qt::NoPen);
    painter.setBrush(appkit::theme::kGuia);
    painter.drawEllipse(c, 3.5, 3.5); // el centro
    appkit::guides::passPoint(painter, major1);
    appkit::guides::passPoint(painter, major2);
    degreeLabel(painter, toCanvas({0, labelSide * (b + kLabelGap)}), degrees);
    return out;
}

} // namespace ejercicios
