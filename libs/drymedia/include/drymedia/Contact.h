#pragma once

#include "drymedia/Medium.h"
#include "drymedia/Tip.h"

#include <cstdint>
#include <vector>

namespace drymedia {

class Paper;

// Contacto de la punta con el papel (plan, "Contacto y depósito"): para una posición, la
// profundidad a la que la fuerza total de contacto iguala la presión aplicada. Con poca
// presión toca solo las crestas del relieve; con más, llega a los valles.
//
// Aritmética entera de 16 bits con saturación, igual en las dos rutas (AVX2 y escalar):
//   superficie = sat(relieve + kBase + depósito / 16)
//   penetración = sat0(superficie − sat(altura de la punta + D))
//   fuerza(D) = Σ penetración  (decrece con D; se busca el mayor D con fuerza ≥ objetivo)
class Contact {
public:
    enum class Path { Auto, Scalar, Avx2 };

    // Auto elige AVX2 si la CPU lo tiene. Pedir Avx2 sin soporte cae a Scalar.
    explicit Contact(Path path = Path::Auto);

    static bool avx2Available();
    Path path() const { return m_path; }

    // Profundidad de contacto con la punta centrada en la celda (x, y) y presión 0..1.
    // No modifica el papel ni crea tiles. Después de llamarla, penetration() tiene la
    // penetración de cada celda de la punta (kTipCells valores).
    uint16_t find(const Paper& paper, const Tip& tip, const Medium& medium, int x, int y, float pressure);

    const uint16_t* penetration() const { return m_penetration.data(); }
    const uint16_t* surface() const { return m_surface.data(); }

    // Depósito de la huella copiado del papel en find(). applyDeposit() le suma el aporte
    // de la penetración actual con el factor k (∝ distancia × blandura), saturando contra el
    // techo de la mina; el llamador lo vuelve a escribir en los tiles (Pencil).
    const uint16_t* deposit() const { return m_deposit.data(); }
    void applyDeposit(uint16_t k, uint16_t ceiling = 65535);
    uint32_t force() const { return m_force; }
    int cellsInContact() const;

    static constexpr uint16_t kBase = 16384; // superficie desplazada: D tiene rango hacia abajo

private:
    Path m_path;
    std::vector<uint16_t> m_relief, m_deposit, m_surface, m_penetration;
    uint32_t m_force = 0;
};

} // namespace drymedia
