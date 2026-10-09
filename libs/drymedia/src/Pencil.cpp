#include "drymedia/Pencil.h"

#include "drymedia/Paper.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace drymedia {

namespace {

constexpr int64_t kOne = 256; // una celda en punto fijo

int64_t floorDiv(int64_t a, int64_t b)
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

} // namespace

void DirtyRect::unite(const DirtyRect& o)
{
    if (o.empty())
        return;
    if (empty()) {
        *this = o;
        return;
    }
    x0 = std::min(x0, o.x0);
    y0 = std::min(y0, o.y0);
    x1 = std::max(x1, o.x1);
    y1 = std::max(y1, o.y1);
}

Pencil::Pencil(Paper& paper, const Medium& medium, Contact::Path path)
    : m_paper(paper)
    , m_medium(medium)
    , m_contact(path)
    , m_tips(360 * 91)
    , m_tileMarked(size_t(paper.tilesX()) * size_t(paper.tilesY()), false)
{
}

Pencil::~Pencil() = default;

void Pencil::setMedium(const Medium& medium)
{
    m_medium = medium;
    for (auto& tip : m_tips)
        tip.reset();
}

Pencil::FixedSample Pencil::toFixed(const PencilSample& s)
{
    FixedSample f;
    f.x = std::llround(s.x * kOne);
    f.y = std::llround(s.y * kOne);
    f.pressure = std::clamp(s.pressure, 0.0f, 1.0f);
    f.azimuth = s.azimuth;
    f.altitude = std::clamp(s.altitude, 0.0f, 90.0f);
    return f;
}

const Tip& Pencil::tipFor(float azimuth, float altitude)
{
    // Ángulos redondeados a 1°: la punta es la misma ante diferencias mínimas de coma
    // flotante, y se genera una sola vez por ángulo.
    int az = int(std::lround(azimuth)) % 360;
    if (az < 0)
        az += 360;
    const int alt = std::clamp(int(std::lround(altitude)), 0, 90);
    std::unique_ptr<Tip>& slot = m_tips[size_t(az) * 91 + size_t(alt)];
    if (!slot)
        slot = std::make_unique<Tip>(Tip::make(m_medium, float(az), float(alt)));
    return *slot;
}

void Pencil::beginStroke(const PencilSample& sample)
{
    m_last = toFixed(sample);
    m_inStroke = true;
    for (const int t : m_strokeTiles)
        m_tileMarked[size_t(t)] = false;
    m_strokeTiles.clear();
}

void Pencil::endStroke()
{
    m_inStroke = false;
}

DirtyRect Pencil::strokeTo(const PencilSample& sample)
{
    DirtyRect dirty;
    if (!m_inStroke)
        return dirty;
    const FixedSample a = m_last;
    const FixedSample b = toFixed(sample);
    m_last = b;

    const int64_t dx = b.x - a.x, dy = b.y - a.y;
    const int64_t chebyshev = std::max(std::llabs(dx), std::llabs(dy));
    if (chebyshev == 0)
        return dirty; // sin desplazamiento no hay deslizamiento: no deposita
    // Subpasos de como mucho una celda en cada eje.
    const int64_t steps = std::max<int64_t>(1, (chebyshev + kOne - 1) / kOne);
    // k ∝ distancia euclídea de cada subpaso (no Chebyshev: si no, en diagonal
    // depositaría de menos) × blandura.
    const double stepLength = std::sqrt(double(dx) * double(dx) + double(dy) * double(dy)) / double(steps);
    const uint16_t k = uint16_t(std::min(65535.0, std::round(stepLength * m_medium.softness)));

    // Azimut por el camino más corto.
    float dAz = b.azimuth - a.azimuth;
    while (dAz > 180.0f)
        dAz -= 360.0f;
    while (dAz < -180.0f)
        dAz += 360.0f;

    for (int64_t s = 1; s <= steps; ++s) {
        const float t = float(s) / float(steps);
        const int64_t x = a.x + dx * s / steps;
        const int64_t y = a.y + dy * s / steps;
        const float pressure = a.pressure + (b.pressure - a.pressure) * t;
        const Tip& tip = tipFor(a.azimuth + dAz * t, a.altitude + (b.altitude - a.altitude) * t);
        dirty.unite(depositAt(x, y, pressure, tip, k));
    }
    m_substeps += uint64_t(steps);
    return dirty;
}

DirtyRect Pencil::depositAt(int64_t fx, int64_t fy, float pressure, const Tip& tip, uint16_t k)
{
    const int cx = int(floorDiv(fx, kOne)), cy = int(floorDiv(fy, kOne));
    m_contact.find(m_paper, tip, m_medium, cx, cy, pressure);
    m_contact.applyDeposit(k, m_medium.ceiling);

    // Escribir el depósito de vuelta en los tiles, solo en los tramos con contacto (así no
    // se crean tiles donde la punta no tocó).
    const int s = Tip::kTipSize;
    const int x0 = cx - Tip::kTipCenter, y0 = cy - Tip::kTipCenter;
    const uint16_t* pen = m_contact.penetration();
    const uint16_t* dep = m_contact.deposit();
    DirtyRect dirty;
    for (int r = 0; r < s; ++r) {
        const int gy = y0 + r;
        if (gy < 0 || gy >= m_paper.height())
            continue;
        int c = std::max(0, -x0);
        const int cEnd = std::min(s, m_paper.width() - x0);
        while (c < cEnd) {
            const int gx = x0 + c;
            const int lx = gx % kTileSize;
            const int run = std::min(cEnd - c, kTileSize - lx);
            const uint16_t* penRun = pen + size_t(r) * s + size_t(c);
            if (std::any_of(penRun, penRun + run, [](uint16_t v) { return v > 0; })) {
                const int tx = gx / kTileSize, ty = gy / kTileSize;
                const int index = ty * m_paper.tilesX() + tx;
                if (!m_tileMarked[size_t(index)]) {
                    // Primera escritura del trazo en este tile: avisar antes de tocarlo.
                    if (m_observer)
                        m_observer(index, m_paper.findDepositTile(tx, ty));
                    m_tileMarked[size_t(index)] = true;
                    m_strokeTiles.push_back(index);
                }
                uint16_t* tile = m_paper.depositTile(tx, ty);
                std::memcpy(tile + size_t(gy % kTileSize) * kTileSize + size_t(lx), dep + size_t(r) * s + size_t(c),
                            size_t(run) * 2);
                dirty.unite({gx, gy, gx + run, gy + 1});
            }
            c += run;
        }
    }
    return dirty;
}

} // namespace drymedia
