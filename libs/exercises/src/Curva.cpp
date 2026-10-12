#include "exercises/Curva.h"

#include <appkit/Guides.h>
#include <appkit/Theme.h>

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

namespace ejercicios {

namespace {

const QString kDistMin = QStringLiteral("distMin");
const QString kDistMax = QStringLiteral("distMax");
const QString kDevMin = QStringLiteral("devMin");
const QString kDevMax = QStringLiteral("devMax");

qreal length(QPointF p)
{
    return QLineF(QPointF(), p).length();
}

} // namespace

QString Curva::id() const
{
    return QStringLiteral("curva");
}

QString Curva::title() const
{
    return QStringLiteral("Tres puntos → curva");
}

QString Curva::group() const
{
    return QStringLiteral("Curvas");
}

QList<appkit::Param> Curva::params() const
{
    using Type = appkit::Param::Type;
    return {
        {.key = kDistMin, .label = QStringLiteral("Largo mínimo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.30},
        {.key = kDistMax, .label = QStringLiteral("Largo máximo (× diámetro de la zona)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 1.0, .step = 0.05, .defaultValue = 0.75},
        {.key = kDevMin, .label = QStringLiteral("Desvío mínimo (× largo)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 0.5, .step = 0.05, .defaultValue = 0.10},
        {.key = kDevMax, .label = QStringLiteral("Desvío máximo (× largo)"), .type = Type::Real,
         .minimum = 0.05, .maximum = 0.5, .step = 0.05, .defaultValue = 0.35},
    };
}

Generated Curva::generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const
{
    QRandomGenerator rng(seed);
    const QVariantMap v = appkit::clampValues(this->params(), params);
    const qreal distMin = v.value(kDistMin).toDouble();
    const qreal distMax = qMax(distMin, v.value(kDistMax).toDouble());
    const qreal devMin = v.value(kDevMin).toDouble();
    const qreal devMax = qMax(devMin, v.value(kDevMax).toDouble());

    // Los anillos de las guías también tienen que entrar en la zona.
    const qreal rTarget = zone.radius - appkit::theme::kRadioPuntoUnir - 2;
    const qreal rPass = zone.radius - appkit::theme::kRadioPuntoPaso - 2;
    qreal largo = qMin(2 * rTarget, (distMin + rng.generateDouble() * (distMax - distMin)) * 2 * zone.radius);
    const qreal desvio = devMin + rng.generateDouble() * (devMax - devMin);
    const qreal lado = rng.bounded(2) ? 1.0 : -1.0;

    // En el marco del ejercicio la cuerda es horizontal (la orientación de la zona la rota):
    // extremos en (±largo/2, 0), punto de paso a `desvio·largo` hacia un lado y el control de
    // la cuadrática al doble. Se busca un corrimiento al azar con todo adentro (el control
    // también: así la curva entera queda en la zona); si no aparece, se achica la figura, que
    // conserva la proporción y con ella el desvío pedido.
    QPointF a, b, mid, control;
    for (bool placed = false; !placed;) {
        const qreal h = desvio * largo * lado;
        for (int attempt = 0; attempt < 60 && !placed; ++attempt) {
            const QPointF offset(rng.generateDouble() * 2 * zone.radius - zone.radius,
                                 rng.generateDouble() * 2 * zone.radius - zone.radius - h);
            a = offset + QPointF(-largo / 2, 0);
            b = offset + QPointF(largo / 2, 0);
            mid = offset + QPointF(0, h);
            control = offset + QPointF(0, 2 * h);
            placed = length(a) <= rTarget && length(b) <= rTarget && length(mid) <= rPass
                     && length(control) <= zone.radius;
        }
        if (!placed)
            largo *= 0.9;
    }

    a = zone.toCanvas(a);
    b = zone.toCanvas(b);
    mid = zone.toCanvas(mid);
    control = zone.toCanvas(control);

    Generated out;
    QPainterPath ideal(a);
    ideal.quadTo(control, b);
    out.ideal.append(ideal);
    QPainter painter(&out.guides);
    painter.setRenderHint(QPainter::Antialiasing);
    appkit::guides::targetPoint(painter, a);
    appkit::guides::targetPoint(painter, b);
    appkit::guides::passPoint(painter, mid);
    return out;
}

} // namespace ejercicios
