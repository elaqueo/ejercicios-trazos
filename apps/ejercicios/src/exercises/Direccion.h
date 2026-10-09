#pragma once

#include "exercises/Exercise.h"

namespace ejercicios {

// HU-25 · Dirección forzada: dos extremos y, al costado, una flecha ámbar con el sentido del
// trazo, para practicar las direcciones incómodas. Las 8 direcciones de la rosa se miden
// sobre la hoja (no con la orientación al azar de la zona): lo incómodo es el movimiento de
// la mano sobre la tableta. Parámetros: distMin y distMax (como la recta), una casilla por
// dirección (si no hay ninguna, valen todas) y jitter (±15° al azar).
// Ideal: la recta orientada de la partida a la llegada.
class Direccion : public Exercise {
public:
    // Direcciones en el orden de las casillas, en grados sobre la hoja (y hacia abajo:
    // 0 = →, 90 = ↓).
    static constexpr int kDirectionCount = 8;
    static int directionDegrees(int index) { return 45 * index; }
    static QString directionKey(int index);

    QString id() const override;
    QString title() const override;
    QString group() const override;
    QList<appkit::Param> params() const override;
    Generated generate(const QVariantMap& params, quint32 seed, const SafeZone& zone) const override;
};

} // namespace ejercicios
