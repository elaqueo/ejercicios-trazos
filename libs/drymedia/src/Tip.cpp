#include "drymedia/Tip.h"

#include "drymedia/Paper.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace drymedia {

namespace {

constexpr double kRadians = std::numbers::pi / 180.0;
// El lápiz más acostado queda 1° por encima del costado del cono: con el costado justo
// sobre el papel toda la generatriz tocaría a la vez.
constexpr double kFlatMarginDeg = 1.0;

} // namespace

double Tip::effectiveAltitude(const Medium& medium, double tabletAltitudeDeg)
{
    // Lineal de vertical (90°) a acostado (en la máxima inclinación de la tableta): sin
    // esto, la tableta llega a ~30° y el costado de un cono de 12° nunca tocaría.
    const double flat = std::clamp(medium.coneHalfAngleDeg, 1.0, 45.0) + kFlatMarginDeg;
    const double limit = std::clamp(medium.minTabletAltitudeDeg, 1.0, 89.0);
    const double t = std::clamp((90.0 - tabletAltitudeDeg) / (90.0 - limit), 0.0, 1.0);
    return 90.0 - t * (90.0 - flat);
}

Tip Tip::make(const Medium& medium, float azimuthDeg, float altitudeDeg)
{
    if (medium.kind == Medium::Kind::Eraser)
        return makeEraser(medium);
    Tip tip;
    const double radius = medium.leadDiameterMm * kCellsPerMm / 2.0;
    const double altitude = effectiveAltitude(medium, altitudeDeg);
    if (altitude >= 90.0) {
        // Vertical: el cono de siempre, idéntico al de la Fase 1.
        tip.m_heights.assign(size_t(kTipCells), kNoContact);
        for (int y = 0; y < kTipSize; ++y)
            for (int x = 0; x < kTipSize; ++x) {
                const double rho = std::hypot(x + 0.5 - kTipCenter, y + 0.5 - kTipCenter);
                if (rho <= radius)
                    tip.m_heights[size_t(y) * kTipSize + size_t(x)] =
                        uint16_t(std::min(65534.0, std::round(rho * medium.coneSlope)));
            }
        return tip;
    }

    // Cono inclinado: vértice en el origen del mapa, eje hacia donde se inclina el cuerpo
    // del lápiz (el azimut de tabletinput). Para cada celda, la altura de la cara de abajo
    // del cono sobre esa celda; la mina llega hasta su radio, a `length` del vértice sobre
    // el eje (más allá es madera, que no marca). La huella empieza en el vértice y crece
    // hacia el cuerpo: al acostar el lápiz apoya el costado.
    const double theta = std::clamp(medium.coneHalfAngleDeg, 1.0, 45.0) * kRadians;
    const double c = std::cos(theta), c2 = c * c;
    const double length = radius / std::tan(theta);
    // z en celdas → unidades de relieve. Con el lápiz vertical z = rho / tan θ, así que esto
    // da rho × coneSlope, como el cono vertical.
    const double toHeight = medium.coneSlope * std::tan(theta);
    const double sAlt = std::sin(altitude * kRadians), cAlt = std::cos(altitude * kRadians);
    const double cAz = std::cos(double(azimuthDeg) * kRadians), sAz = std::sin(double(azimuthDeg) * kRadians);
    const double ax = cAlt * cAz, ay = cAlt * sAz;

    // Caja de la huella, ajustada (el costado de la 6B mide ~80 celdas de largo y no más de
    // 35 de ancho; un mapa cuadrado centrado en el vértice cuadruplicaba el contacto): el
    // vértice y la elipse que es la punta del cono de mina vista desde arriba, de semiejes
    // radio·sen(alt) a lo largo del azimut y radio a lo ancho.
    const double ex = length * ax, ey = length * ay;
    const double along = radius * sAlt;
    const double hx = std::hypot(along * cAz, radius * sAz), hy = std::hypot(along * sAz, radius * cAz);
    const double minX = std::min(0.0, ex - hx) - 1.0, maxX = std::max(0.0, ex + hx) + 1.0;
    const double minY = std::min(0.0, ey - hy) - 1.0, maxY = std::max(0.0, ey + hy) + 1.0;
    tip.m_originX = int(std::ceil(-minX));
    tip.m_originY = int(std::ceil(-minY));
    tip.m_width = (tip.m_originX + int(std::ceil(maxX)) + 3) / 4 * 4;
    tip.m_height = (tip.m_originY + int(std::ceil(maxY)) + 3) / 4 * 4;
    tip.m_heights.assign(size_t(tip.cells()), kNoContact);
    const double a = sAlt * sAlt - c2; // ≠ 0 salvo en un ángulo exacto; ver abajo
    for (int y = 0; y < tip.m_height; ++y)
        for (int x = 0; x < tip.m_width; ++x) {
            const double px = x + 0.5 - tip.m_originX, py = y + 0.5 - tip.m_originY;
            // Punto (px, py, z): dentro del cono si (u + z·sen alt)² ≥ cos²θ·(px² + py² + z²),
            // con u + z·sen alt ≥ 0 (la hoja de arriba). Cuadrática en z.
            const double u = px * ax + py * ay;
            const double b = 2.0 * u * sAlt, cc = u * u - c2 * (px * px + py * py);
            double z;
            if (std::abs(a) < 1e-12) {
                if (b <= 0.0)
                    continue;
                z = -cc / b;
            } else {
                const double disc = b * b - 4.0 * a * cc;
                if (disc < 0.0)
                    continue;
                const double q = -0.5 * (b + std::copysign(std::sqrt(disc), b));
                const double z1 = q / a, z2 = q != 0.0 ? cc / q : z1;
                // La cara de abajo es la raíz más baja de la hoja de arriba (la otra, si es de
                // la misma hoja, es la cara de arriba).
                const bool up1 = u + z1 * sAlt >= 0.0, up2 = u + z2 * sAlt >= 0.0;
                if (!up1 && !up2)
                    continue;
                z = up1 && up2 ? std::min(z1, z2) : (up1 ? z1 : z2);
            }
            if (u + z * sAlt > length)
                continue; // ya es madera
            tip.m_heights[size_t(y) * size_t(tip.m_width) + size_t(x)] =
                uint16_t(std::clamp(std::round(z * toHeight), 0.0, 65534.0));
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
    tip.m_width = tip.m_height = (int(std::ceil(2.0 * radius)) + 2 + 3) / 4 * 4;
    tip.m_originX = tip.m_originY = tip.m_width / 2;
    tip.m_heights.assign(size_t(tip.cells()), kNoContact);
    const double c = tip.m_originX;
    for (int y = 0; y < tip.m_height; ++y)
        for (int x = 0; x < tip.m_width; ++x) {
            const double rho = std::hypot(x + 0.5 - c, y + 0.5 - c);
            if (rho <= radius) {
                const double out = std::max(0.0, rho - (radius - edge));
                tip.m_heights[size_t(y) * size_t(tip.m_width) + size_t(x)] =
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
