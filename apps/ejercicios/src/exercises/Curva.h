#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-22 · Tres puntos → curva: dos extremos y un punto de paso para unirlos con un solo
// trazo curvo. La curva ideal es la parábola que pasa por los tres (Bézier cuadrática con el
// punto de paso en t = 0,5). Parámetros: distMin y distMax, largo entre extremos como
// fracción del diámetro de la zona; devMin y devMax, cuánto se aparta el punto de paso de la
// recta entre los extremos, como fracción de ese largo.
class Curva : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
