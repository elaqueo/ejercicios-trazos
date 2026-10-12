#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-26 · Elipses por grado: el grado es el ángulo con que se mira un círculo (eje menor =
// eje mayor · sen grado; 90° sería un círculo). Guías: el centro, el eje menor como
// construcción que atraviesa la elipse, los extremos del eje mayor como puntos de paso (el
// tamaño) y el grado escrito en ámbar. Parámetros: una casilla por grado (15° a 75°; sin
// ninguna, todos) y sizeMin/sizeMax (eje mayor como fracción del diámetro de la zona).
// Ideal: la elipse, su eje mayor y su eje menor.
class Elipse : public Exercise {
public:
    static constexpr int kDegrees[] = {15, 30, 45, 60, 75};
    static QString degreeKey(int degrees);

    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
