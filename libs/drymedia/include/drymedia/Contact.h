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
//   r = relieve / 2^reliefShift
//   superficie = sat(r + kBase + (crestas de la zona / 2^reliefShift − r) · depósito / 65536)
//   (antes, la fibra dañada vuelve más rugoso a r y el bruñido aplasta sus crestas hacia el
//   nivel medio de la zona, HU-60 y HU-61; después se resta la deformación)
//   (el grafito llena el diente: saturado, la celda queda al nivel de las crestas; antes
//   sumaba depósito / 16 y una celda saturada subía un diente entero por encima del papel,
//   así que la mina de costado se apoyaba en ella y no volvía a tocar el resto, HU-59)
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
    // penetración de cada celda de la punta (tip.cells() valores).
    uint16_t find(const Paper& paper, const Tip& tip, const Medium& medium, int x, int y, float pressure);

    const uint16_t* penetration() const { return m_penetration.data(); }
    const uint16_t* surface() const { return m_surface.data(); }

    // Depósito de la huella copiado del papel en find(). applyDeposit() le suma el aporte
    // de la penetración actual con el factor k (∝ distancia × blandura), saturando contra el
    // techo de la mina; el llamador lo vuelve a escribir en los tiles (Pencil).
    const uint16_t* deposit() const { return m_deposit.data(); }
    void applyDeposit(uint16_t k, uint16_t ceiling = 65535);
    // Goma (HU-58): quita del depósito de la huella ∝ penetración × k; llega a 0.
    void applyErase(uint16_t k);
    // Bruñido (HU-60): crece con la penetración × kb (∝ distancia × presión³ × tasa) donde hay
    // grafito, y sube el depósito de las celdas en contacto que están por debajo de su
    // promedio: arrastra grafito a los valles, nunca aclara.
    // burnish() es el bruñido de la huella (copiado en find(); el llamador lo vuelve a escribir).
    void applyBurnish(uint16_t kb);
    const uint16_t* burnish() const { return m_burnish.data(); }
    // Deformación (HU-61): si la penetración media del contacto pasa kYield (el papel cede),
    // cada celda en contacto se hunde de forma permanente ∝ su penetración × rate × el exceso
    // relativo, hasta kMaxDeform (dos dientes).
    void applyDeform(uint16_t rate);
    uint32_t meanPenetration() const;
    // Daño de fibra (HU-61, borrar de más): crece ∝ penetración × kd.
    void applyDamage(uint16_t kd);
    const uint16_t* deform() const { return m_deform.data(); }
    const uint16_t* damage() const { return m_damage.data(); }
    // Los planos del tile en el orden de Paper (depósito, bruñido, deformación, daño).
    const uint16_t* plane(int index) const
    {
        const std::vector<uint16_t>* planes[] = {&m_deposit, &m_burnish, &m_deform, &m_damage};
        return planes[index]->data();
    }
    uint32_t force() const { return m_force; }
    int cellsInContact() const;

    static constexpr uint16_t kBase = 16384; // superficie desplazada: D tiene rango hacia abajo
    static constexpr uint16_t kYield = 2000;      // penetración media a la que el papel cede (HU-61)
    static constexpr uint16_t kMaxDeform = 8192;  // hundimiento máximo: dos dientes

private:
    Path m_path;
    int m_cells = 0; // celdas de la última punta (los arreglos tienen ese tamaño)
    std::vector<uint16_t> m_relief, m_crest, m_deposit, m_burnish, m_deform, m_damage, m_surface, m_penetration;
    uint32_t m_force = 0;
};

} // namespace drymedia
