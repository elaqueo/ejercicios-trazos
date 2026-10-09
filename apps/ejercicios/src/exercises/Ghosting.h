#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-35 · Ghosting con memoria: una forma completa (la geometría ideal de la recta, la curva
// de tres puntos o la elipse por grado) se ve unos segundos y después se oculta, para
// dibujarla de memoria; G la vuelve a mostrar para comparar (lo maneja la sesión con
// Generated::visibleMs). Parámetros: seconds (tiempo visible), recta, curva, elipse (formas
// habilitadas; sin ninguna, todas) y size (fracción del diámetro de la zona).
// Ideal: la forma.
class Ghosting : public Exercise {
public:
    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
