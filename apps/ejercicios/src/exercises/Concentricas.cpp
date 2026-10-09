#include "exercises/Concentricas.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QFont>
#include <QPainter>
#include <QRandomGenerator>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ejercicios {

namespace {

const QString kCount = QStringLiteral("count");
const QString kDegMin = QStringLiteral("degMin");
const QString kDegMax = QStringLiteral("degMax");
const QString kWidthMin = QStringLiteral("widthMin");
const QString kWidthMax = QStringLiteral("widthMax");
const QString kShuffle = QStringLiteral("shuffle");

constexpr qreal kGap = 12;        // entre elipses vecinas
constexpr qreal kAxisOver = 24;   // el eje sobresale de la primera y la última
constexpr qreal kLabelGap = 44;   // del extremo derecho al centro del rótulo
constexpr qreal kLabelHalf = 36;  // medio ancho del rótulo

void degreeLabel(QPainter& painter, QPointF at, int degrees)
{
    painter.save();
    QFont font(QString::fromLatin1(appkit::theme::kFuenteMono));
    font.setPixelSize(20);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(appkit::theme::kEnfasis);
    painter.drawText(QRectF(at.x() - kLabelHalf, at.y() - 14, 2 * kLabelHalf, 28), Qt::AlignCenter,
                     QStringLiteral("%1°").arg(degrees));
    painter.restore();
}

} // namespace

QString Concentricas::id() const
{
    return QStringLiteral("concentricas");
}

QString Concentricas::title() const
{
    return QStringLiteral("Elipses concéntricas sobre un eje");
}

QString Concentricas::group() const
{
    return QStringLiteral("Elipses");
}

QList<appkit::Param> Concentricas::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kCount, .label = QStringLiteral("Cantidad de elipses"), .type = Type::Integer, .minimum = 2,
         .maximum = 5, .step = 1, .defaultValue = 3},
        {.key = kDegMin, .label = QStringLiteral("Grado mínimo"), .type = Type::Integer, .minimum = 10, .maximum = 80,
         .step = 5, .defaultValue = 15, .suffix = QStringLiteral("°")},
        {.key = kDegMax, .label = QStringLiteral("Grado máximo"), .type = Type::Integer, .minimum = 10, .maximum = 80,
         .step = 5, .defaultValue = 60, .suffix = QStringLiteral("°")},
        {.key = kWidthMin, .label = QStringLiteral("Ancho mínimo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.1, .maximum = 0.7, .step = 0.05, .defaultValue = 0.30},
        {.key = kWidthMax, .label = QStringLiteral("Ancho máximo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.1, .maximum = 0.7, .step = 0.05, .defaultValue = 0.50},
        {.key = kShuffle, .label = QStringLiteral("Grados en desorden"), .type = Type::Toggle, .defaultValue = false},
    };
}

Generated Concentricas::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const int count = v.value(kCount).toInt();
    const int degMin = std::min(v.value(kDegMin).toInt(), v.value(kDegMax).toInt());
    const int degMax = std::max(v.value(kDegMin).toInt(), v.value(kDegMax).toInt());
    const qreal widthMin = v.value(kWidthMin).toDouble();
    const qreal widthMax = qMax(widthMin, v.value(kWidthMax).toDouble());

    // Grados en progresión, redondeados a 5°; el sentido al azar, o en desorden.
    QList<int> degrees;
    for (int i = 0; i < count; ++i)
        degrees.append(int(std::lround((degMin + (degMax - degMin) * qreal(i) / (count - 1)) / 5.0)) * 5);
    const bool reversed = rng.bounded(2);
    if (reversed)
        std::reverse(degrees.begin(), degrees.end());
    if (v.value(kShuffle).toBool())
        for (int i = count - 1; i > 0; --i)
            std::swap(degrees[i], degrees[int(rng.bounded(quint32(i + 1)))]);

    // Marco del ejercicio: el eje vertical, los ejes mayores horizontales (la orientación de
    // la zona los rota). Si no entra (puntos de control de las elipses, rótulos y eje), se
    // achica el ancho.
    qreal a = (widthMin + rng.generateDouble() * (widthMax - widthMin)) * zone.radius;
    QList<qreal> b, y;
    qreal reach = 0;
    const qreal margin = 4;
    for (;;) {
        b.clear();
        y.clear();
        qreal total = (count - 1) * kGap;
        for (const int d : degrees) {
            b.append(a * std::sin(d * std::numbers::pi / 180));
            total += 2 * b.last();
        }
        qreal top = -total / 2;
        for (int i = 0; i < count; ++i) {
            y.append(top + b[i]);
            top += 2 * b[i] + kGap;
        }
        reach = total / 2 + kAxisOver;
        for (int i = 0; i < count; ++i) {
            reach = std::max(reach, std::hypot(a, std::abs(y[i]) + b[i]));
            reach = std::max(reach, std::hypot(a + kLabelGap + kLabelHalf, std::abs(y[i]) + 14));
        }
        if (reach <= zone.radius - margin)
            break;
        a *= 0.95;
    }
    const qreal room = std::max(0.0, zone.radius - margin - reach);
    const qreal along = rng.generateDouble() * 2 * std::numbers::pi;
    const QPointF center = QPointF(std::cos(along), std::sin(along)) * room * std::sqrt(rng.generateDouble());
    const auto toCanvas = [&](QPointF local) { return zone.toCanvas(center + local); };

    Generated out;
    const qreal axisHalf = (y.last() + b.last() - (y.first() - b.first())) / 2 + kAxisOver;
    const QPointF axisFrom = toCanvas({0, -axisHalf}), axisTo = toCanvas({0, axisHalf});
    QPainterPath axis(axisFrom);
    axis.lineTo(axisTo);
    out.ideal.append(axis);

    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::guideLine(painter, axisFrom, axisTo);
    for (int i = 0; i < count; ++i) {
        const QPointF c = toCanvas({0, y[i]});
        QTransform transform;
        transform.translate(c.x(), c.y());
        transform.rotateRadians(zone.orientation);
        QPainterPath ellipse;
        ellipse.addEllipse(QPointF(), a, b[i]);
        out.ideal.append(transform.map(ellipse));

        painter.setPen(Qt::NoPen);
        painter.setBrush(appkit::theme::kGuia);
        painter.drawEllipse(c, 4.0, 4.0); // el centro, sobre el eje
        appkit::guides::passPoint(painter, toCanvas({-a, y[i]}));
        appkit::guides::passPoint(painter, toCanvas({a, y[i]}));
        degreeLabel(painter, toCanvas({a + kLabelGap, y[i]}), degrees[i]);
    }
    return out;
}

} // namespace ejercicios
