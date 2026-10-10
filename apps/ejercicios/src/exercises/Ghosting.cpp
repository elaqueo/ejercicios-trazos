#include "exercises/Ghosting.h"

#include "exercises/Curva.h"
#include "exercises/Elipse.h"
#include "exercises/Recta.h"

#include <appkit/Guides.h>

#include <QPainter>
#include <QRandomGenerator>

namespace ejercicios {

namespace {

const QString kSeconds = QStringLiteral("seconds");
const QString kRecta = QStringLiteral("recta");
const QString kCurva = QStringLiteral("curva");
const QString kElipse = QStringLiteral("elipse");
const QString kSize = QStringLiteral("size");

} // namespace

QString Ghosting::id() const
{
    return QStringLiteral("ghosting");
}

QString Ghosting::title() const
{
    return QStringLiteral("Ghosting con memoria");
}

QString Ghosting::group() const
{
    return QStringLiteral("Memoria y presión"); // un grupo en el menú (mesa del diseño, HU-77)
}

QList<appkit::Param> Ghosting::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kSeconds, .label = QStringLiteral("Tiempo visible"), .type = Type::Real, .minimum = 1, .maximum = 10,
         .step = 0.5, .defaultValue = 3.0, .decimals = 1, .suffix = QStringLiteral(" s")},
        {.key = kRecta, .label = QStringLiteral("Recta"), .type = Type::Toggle, .defaultValue = true},
        {.key = kCurva, .label = QStringLiteral("Curva"), .type = Type::Toggle, .defaultValue = true},
        {.key = kElipse, .label = QStringLiteral("Elipse"), .type = Type::Toggle, .defaultValue = true},
        {.key = kSize, .label = QStringLiteral("Tamaño (× diámetro de la zona)"), .type = Type::Real, .minimum = 0.3,
         .maximum = 0.8, .step = 0.05, .defaultValue = 0.5},
    };
}

Generated Ghosting::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    QStringList shapes;
    for (const QString& key : {kRecta, kCurva, kElipse})
        if (v.value(key).toBool())
            shapes.append(key);
    if (shapes.isEmpty())
        shapes = {kRecta, kCurva, kElipse};
    const QString shape = shapes[int(rng.bounded(quint32(shapes.size())))];
    const qreal size = v.value(kSize).toDouble();
    const quint32 shapeSeed = rng.generate();

    // La forma sale del ejercicio que ya la genera, con el tamaño pedido (±10 %).
    Generated source;
    if (shape == kRecta) {
        const Recta recta;
        QVariantMap p = recta.defaults();
        p[QStringLiteral("distMin")] = size * 0.9;
        p[QStringLiteral("distMax")] = size;
        source = recta.generate(p, shapeSeed, zone);
    } else if (shape == kCurva) {
        const Curva curva;
        QVariantMap p = curva.defaults();
        p[QStringLiteral("distMin")] = size * 0.9;
        p[QStringLiteral("distMax")] = size;
        source = curva.generate(p, shapeSeed, zone);
    } else {
        const Elipse elipse;
        QVariantMap p = elipse.defaults();
        p[QStringLiteral("sizeMin")] = size * 0.9;
        p[QStringLiteral("sizeMax")] = size;
        source = elipse.generate(p, shapeSeed, zone);
    }

    Generated out;
    out.ideal = {source.ideal.first()};
    out.visibleMs = int(v.value(kSeconds).toDouble() * 1000);
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::guidePath(painter, out.ideal.first());
    return out;
}

} // namespace ejercicios
