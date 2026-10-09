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

int Tip::cellsInside() const
{
    return int(std::count_if(m_heights.begin(), m_heights.end(), [](uint16_t h) { return h != kNoContact; }));
}

} // namespace drymedia
