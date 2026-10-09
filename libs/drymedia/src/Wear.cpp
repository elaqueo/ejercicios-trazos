#include "drymedia/Wear.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace drymedia {

void LeadWear::setup(double radiusCells, double halfAngleDeg)
{
    if (m_size && radiusCells == m_radius && halfAngleDeg == m_halfAngle)
        return;
    m_radius = radiusCells;
    m_halfAngle = halfAngleDeg;
    m_size = 2 * int(std::ceil(radiusCells)) + 4;
    m_center = m_size / 2;
    m_cap.assign(size_t(m_size) * size_t(m_size), 0);
    // Tope: la cara del cono (a ρ / tan θ del vértice) puede bajar hasta la base del cono
    // de mina (radio / tan θ): plana al ras.
    const double tanTheta = std::tan(std::clamp(halfAngleDeg, 1.0, 45.0) * std::numbers::pi / 180.0);
    for (int y = 0; y < m_size; ++y)
        for (int x = 0; x < m_size; ++x) {
            const double rho = std::hypot(x - m_center, y - m_center);
            if (rho <= radiusCells)
                m_cap[size_t(y) * size_t(m_size) + size_t(x)] =
                    uint32_t(std::lround((radiusCells - rho) / tanTheta * kUnit));
        }
    reset();
}

void LeadWear::reset()
{
    m_wear.assign(m_cap.size(), 0);
    m_snapshot.assign(m_cap.size(), 0);
    m_changed = false;
    m_any = false;
}

int LeadWear::index(double u, double v) const
{
    const int x = int(std::lround(u)) + m_center, y = int(std::lround(v)) + m_center;
    if (x < 0 || y < 0 || x >= m_size || y >= m_size)
        return -1;
    const int i = y * m_size + x;
    return m_cap[size_t(i)] ? i : -1;
}

double LeadWear::at(double u, double v) const
{
    if (!m_any)
        return 0;
    const double fx = u + m_center, fy = v + m_center;
    const int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
    const double tx = fx - x0, ty = fy - y0;
    const auto value = [&](int x, int y) -> double {
        if (x < 0 || y < 0 || x >= m_size || y >= m_size)
            return 0;
        return m_wear[size_t(y) * size_t(m_size) + size_t(x)];
    };
    const double w = (value(x0, y0) * (1 - tx) + value(x0 + 1, y0) * tx) * (1 - ty) +
                     (value(x0, y0 + 1) * (1 - tx) + value(x0 + 1, y0 + 1) * tx) * ty;
    return w / kUnit;
}

void LeadWear::add(int index, uint64_t amount)
{
    if (index < 0 || amount == 0)
        return;
    uint32_t& w = m_wear[size_t(index)];
    const uint32_t cap = m_cap[size_t(index)];
    w = uint32_t(std::min<uint64_t>(uint64_t(w) + amount, cap));
    m_any = true;
    if (w - m_snapshot[size_t(index)] > kRegenerate)
        m_changed = true;
}

bool LeadWear::takeChanged()
{
    if (!m_changed)
        return false;
    m_snapshot = m_wear;
    m_changed = false;
    return true;
}

int LeadWear::percent() const
{
    // El material que se fue respecto del cono de mina entero (de costado se gasta un lado y
    // el vértice puede quedar intacto).
    uint64_t worn = 0, total = 0;
    for (size_t i = 0; i < m_cap.size(); ++i)
        worn += m_wear[i], total += m_cap[i];
    return total ? int(worn * 100 / total) : 0;
}

} // namespace drymedia
