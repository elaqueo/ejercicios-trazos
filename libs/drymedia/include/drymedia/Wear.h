#pragma once

#include <cstdint>
#include <vector>

namespace drymedia {

// Desgaste de una mina (HU-62): cuánto material perdió cada punto de su cara, medido a lo
// largo del eje (en 1/kUnit de celda), sobre una grilla de la sección de la mina en sus
// propias coordenadas (u, v ⊥ al eje). Así el desgaste acompaña al lápiz al inclinarlo.
//
// La base (u, v) queda fija respecto de la hoja: u es el eje x de la hoja proyectado sobre
// el plano ⊥ al eje y v completa la base. Con el lápiz siempre inclinado hacia el mismo lado
// se gasta ese costado (faceta); si la inclinación va girando, se gasta parejo.
//
// Cada punto puede gastarse hasta que la cara queda plana a la altura de la base del cono
// (la mina gastada al ras: hay que afilar). Enteros: el mismo trazo da el mismo desgaste.
class LeadWear {
public:
    static constexpr uint32_t kUnit = 65536; // una celda de desgaste a lo largo del eje

    // Prepara la grilla para una mina de ese radio (celdas) y semiángulo; si cambia la
    // geometría, vuelve a la punta nueva.
    void setup(double radiusCells, double halfAngleDeg);
    void reset(); // afilar: la punta cónica nueva

    bool empty() const { return m_size == 0; }
    int size() const { return m_size; }
    // Índice del punto de la grilla más cercano a (u, v), o -1 fuera de la mina.
    int index(double u, double v) const;
    // Desgaste en (u, v) en celdas, interpolado.
    double at(double u, double v) const;
    // Suma desgaste a un punto, hasta su tope.
    void add(int index, uint64_t amount);
    // true si algún punto se gastó más de kRegenerate desde la última vez que dio true: la
    // punta tiene que regenerarse.
    bool takeChanged();
    // Cuánto se gastó la punta: el material que se fue respecto del cono de mina, 0..100.
    int percent() const;
    bool worn() const { return m_any; }
    const std::vector<uint32_t>& values() const { return m_wear; }

private:
    static constexpr uint32_t kRegenerate = kUnit / 4;
    int m_size = 0, m_center = 0;
    double m_radius = 0, m_halfAngle = 0;
    std::vector<uint32_t> m_wear, m_snapshot, m_cap;
    bool m_changed = false, m_any = false;
};

} // namespace drymedia
