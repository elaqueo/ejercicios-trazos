#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-23 · Hatching: un rectángulo para rellenar con líneas paralelas. El contorno va como
// construcción y adentro dos muestras firmes: una de lado a lado con el ángulo y otra corta,
// paralela, a la distancia del espaciado sugerido (en mm reales: la hoja está a escala).
// Parámetros: size (lado mayor como fracción del diámetro de la zona), spacing (mm),
// randomAngle y angle (grados respecto del rectángulo, en pasos de 15°).
// Ideal: el contorno cerrado y las dos muestras.
class Hatching : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
