#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-36 · Control de presión: una banda de ancho variable a lo largo del recorrido marca la
// presión objetivo (angosta = suave, ancha = fuerte; en el grafito la presión cambia sobre
// todo el tono, así que la banda la representa sin depender del ancho del trazo). Línea
// central fina y punto de arranque. Perfiles: fino → grueso, constante y libre (suave, al
// azar, alternando fino y grueso). Parámetros: profileRamp, profileConstant, profileFree (sin
// ninguno, todos), length (fracción del diámetro de la zona), width (ancho máximo, mm) y
// curved (recorrido curvo).
// Ideal: el recorrido (polilínea desde el arranque) y el contorno de la banda (un lado del
// arranque al final y el otro de vuelta, cerrado).
class Presion : public Exercise {
public:
    static constexpr int kSamples = 64;

    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
