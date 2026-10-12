#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-33 · Escribir sobre una curva: el renglón de HU-32 con la base curva (WritingCurve, C o
// S). La altura de x y el interlineado son curvas paralelas a la base. La curvatura no supera
// 1/minRadius (mm reales) y el radio nunca baja de 1,5 interlineados, para que las guías
// paralelas no se doblen. Parámetros: length, spacing, xHeight, minRadius (mm), allowS,
// randomAngle (±20°) y angle (de la cuerda, sobre la hoja).
// Ideal: la base, la altura de x y el interlineado, como polilíneas de izquierda a derecha.
class RenglonCurvo : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
