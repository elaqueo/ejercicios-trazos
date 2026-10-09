#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-32 · Escribir sobre una recta: un renglón. Guías: la línea base firme, la altura de x y
// el interlineado (techo de las ascendentes, donde iría el renglón siguiente) punteadas, y un
// punto donde empezar. El ángulo se mide sobre la hoja (no con la orientación de la zona),
// para que el texto se lea siempre de izquierda a derecha; las medidas van en mm reales.
// Parámetros: length (fracción del diámetro de la zona), spacing (interlineado, mm),
// xHeight (fracción del interlineado), randomAngle (±20°) y angle (fijo, grados; positivo
// sube hacia la derecha).
// Ideal: la base, la altura de x y el interlineado, cada una de izquierda a derecha.
class Renglon : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
