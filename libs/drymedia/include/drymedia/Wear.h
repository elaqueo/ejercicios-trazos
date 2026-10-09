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
    // index() y at() en una sola cuenta, sin std::floor ni std::lround (en MSVC son llamadas
    // a funciones; Tip lo llama por cada celda al armarse, HU-76). Mismo resultado.
    double sample(double u, double v, int& index) const
    {
        // Más cercano como lround(u) + centro: los empates en .5 (la punta vertical los tiene en
        // todas las celdas) se redondean hacia afuera.
        const int nx = m_center + (u < 0 ? -int(0.5 - u) : int(u + 0.5));
        const int ny = m_center + (v < 0 ? -int(0.5 - v) : int(v + 0.5));
        index = nx >= 0 && ny >= 0 && nx < m_size && ny < m_size && m_cap[size_t(ny) * size_t(m_size) + size_t(nx)]
                    ? ny * m_size + nx
                    : -1;
        if (!m_any)
            return 0;
        const double fx = u + m_center, fy = v + m_center;
        if (fx < 0 || fy < 0)
            return at(u, v);
        const int x0 = int(fx), y0 = int(fy); // = floor: positivos
        if (x0 + 1 >= m_size || y0 + 1 >= m_size)
            return at(u, v);
        const double tx = fx - x0, ty = fy - y0;
        const uint32_t* row = m_wear.data() + size_t(y0) * size_t(m_size) + size_t(x0);
        const double w = (double(row[0]) * (1 - tx) + double(row[1]) * tx) * (1 - ty) +
                         (double(row[m_size]) * (1 - tx) + double(row[m_size + 1]) * tx) * ty;
        return w / kUnit;
    }
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
