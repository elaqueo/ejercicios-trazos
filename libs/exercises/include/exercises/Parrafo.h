#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-34 · Escribir sobre varias curvas: un párrafo que sigue una curva. La primera base es
// una WritingCurve (C o S) y los renglones de abajo son curvas paralelas, cada uno a un
// interlineado del anterior: no se cruzan y el interlineado es exacto en todo el largo. Como
// desplazar una curva hacia su lado cóncavo la cierra, la base se genera con radio
// minRadius + (renglones − 1) · interlineado, y así ningún renglón baja de minRadius.
// Parámetros: lines, length, spacing, xHeight, minRadius (mm), allowS, randomAngle, angle.
// Ideal: por renglón, la base y la altura de x (de arriba hacia abajo, de izquierda a derecha).
class Parrafo : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
