#include "drymedia/Tip.h"

#include "drymedia/Paper.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace drymedia {

namespace {

constexpr double kRadians = std::numbers::pi / 180.0;
// Por debajo de esto el estiramiento se dispara; la Intuos4 llega a ~30°.
constexpr double kMinAltitude = 20.0;

} // namespace

Tip Tip::make(const Medium& medium, float azimuthDeg, float altitudeDeg)
{
    if (medium.kind == Medium::Kind::Eraser)
        return makeEraser(medium);
    Tip tip;
    tip.m_heights.assign(size_t(kTipCells), kNoContact);
    const double radius = medium.leadDiameterMm * kCellsPerMm / 2.0;
    const double stretch = 1.0 / std::sin(std::clamp(double(altitudeDeg), kMinAltitude, 90.0) * kRadians);
    const double ca = std::cos(double(azimuthDeg) * kRadians), sa = std::sin(double(azimuthDeg) * kRadians);

    for (int y = 0; y < kTipSize; ++y)
        for (int x = 0; x < kTipSize; ++x) {
            const double dx = x + 0.5 - kTipCenter, dy = y + 0.5 - kTipCenter;
            const double u = dx * ca + dy * sa;  // a lo largo del azimut
            const double v = -dx * sa + dy * ca; // perpendicular
            const double rho = std::sqrt((u / stretch) * (u / stretch) + v * v);
            if (rho <= radius)
                tip.m_heights[size_t(y) * kTipSize + size_t(x)] =
                    uint16_t(std::min(65534.0, std::round(rho * medium.coneSlope)));
        }
    return tip;
}

Tip Tip::makeEraser(const Medium& medium)
{
    // Cara plana con el borde redondeado (0,3 mm): apoyada, toca primero las crestas del
    // papel en toda su cara; con más presión llega a los valles.
    const double radius = medium.leadDiameterMm * kCellsPerMm / 2.0;
    const double edge = std::min(radius, 0.3 * kCellsPerMm);
    Tip tip;
    tip.m_size = (int(std::ceil(2.0 * radius)) + 2 + 3) / 4 * 4;
    tip.m_heights.assign(size_t(tip.cells()), kNoContact);
    const double c = tip.center();
    for (int y = 0; y < tip.m_size; ++y)
        for (int x = 0; x < tip.m_size; ++x) {
            const double rho = std::hypot(x + 0.5 - c, y + 0.5 - c);
            if (rho <= radius) {
                const double out = std::max(0.0, rho - (radius - edge));
                tip.m_heights[size_t(y) * size_t(tip.m_size) + size_t(x)] =
                    uint16_t(std::min(65534.0, std::round(out * medium.coneSlope)));
            }
        }
    return tip;
}

int Tip::cellsInside() const
{
    return int(std::count_if(m_heights.begin(), m_heights.end(), [](uint16_t h) { return h != kNoContact; }));
}

} // namespace drymedia
