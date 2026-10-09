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
    setupWear();
}

Pencil::~Pencil() = default;

void Pencil::setMedium(const Medium& medium)
{
    m_medium = medium;
    setupWear();
    clearTips();
}

void Pencil::setupWear()
{
    if (wears())
        m_wear->setup(m_medium.leadDiameterMm * kCellsPerMm / 2.0, m_medium.coneHalfAngleDeg);
}

void Pencil::clearTips()
{
    for (const size_t i : m_filledTips)
        m_tips[i].reset();
    m_filledTips.clear();
}

void Pencil::setMedium(const Medium& medium, LeadWear* wear)
{
    m_medium = medium;
    m_wear = wear ? wear : &m_ownWear;
    setupWear();
    clearTips();
}

void Pencil::setWear(LeadWear* wear)
{
    m_wear = wear ? wear : &m_ownWear;
    setupWear();
    clearTips();
}

void Pencil::sharpen()
{
    m_wear->reset();
    clearTips();
}

void Pencil::wearAt(const Tip& tip, uint16_t k)
{
    // Cada punto de la mina que tocó pierde ∝ lo que depositó × blandura: penetración × k ×
    // blandura × wearRate.
    const std::vector<uint16_t>& lead = tip.leadIndex();
    if (lead.empty() || k == 0)
        return;
    const uint16_t* pen = m_contact.penetration();
    const uint64_t scale = uint64_t(k) * m_medium.softness * m_medium.wearRate;
    for (int i = 0; i < tip.cells(); ++i)
        if (pen[i] > 0 && lead[size_t(i)] != Tip::kNoLead)
            m_wear->add(lead[size_t(i)], (uint64_t(pen[i]) * scale) >> 28);
    if (m_wear->takeChanged())
        clearTips(); // la punta cambió: se rearma con el desgaste nuevo
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
    // flotante, y se genera una sola vez por ángulo. La goma no depende del ángulo.
    if (m_medium.kind == Medium::Kind::Eraser)
        azimuth = 0, altitude = 90;
    int az = int(std::lround(azimuth)) % 360;
    if (az < 0)
        az += 360;
    const int alt = std::clamp(int(std::lround(altitude)), 0, 90);
    std::unique_ptr<Tip>& slot = m_tips[size_t(az) * 91 + size_t(alt)];
    if (!slot) {
        slot = std::make_unique<Tip>(Tip::make(m_medium, float(az), float(alt), wears() ? m_wear : nullptr));
        m_filledTips.push_back(size_t(az) * 91 + size_t(alt));
    }
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
    // Subpasos de como mucho maxStepCells celdas en cada eje (una, en las minas).
    const int64_t stepMax = kOne * std::max(1, m_medium.maxStepCells);
    const int64_t steps = std::max<int64_t>(1, (chebyshev + stepMax - 1) / stepMax);
    // k ∝ distancia euclídea de cada subpaso (no Chebyshev: si no, en diagonal
    // depositaría de menos) × blandura.
    const double stepLength = std::sqrt(double(dx) * double(dx) + double(dy) * double(dy)) / double(steps);
    // La goma usa una escala 4 veces más fina (fuerza / 4): sus subpasos son de hasta 4
    // celdas, y con la escala de la mina k se saturaba a partir de fuerza ~64.
    const double scale = m_medium.kind == Medium::Kind::Eraser ? 0.25 : 1.0;
    const uint16_t k = uint16_t(std::min(65535.0, std::round(stepLength * m_medium.softness * scale)));
    // Bruñido (HU-60) y daño de fibra (HU-61): ∝ distancia (stepLength está en 1/256 de celda) × presión³.
    const double burnishStep = stepLength / double(kOne) * m_medium.burnishRate;
    const double damageStep = stepLength / double(kOne) * m_medium.damageRate;

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
        // Goma: además, lo que quita ∝ presión (el roce que levanta el grafito crece con
        // la fuerza normal). Si no, apoyada suave borraba casi como apretando: blanda y
        // ancha, igual se hunde en las crestas.
        const uint16_t kStep = m_medium.kind == Medium::Kind::Eraser
                                   ? uint16_t(std::lround(double(k) * std::clamp(double(pressure), 0.0, 1.0)))
                                   : k;
        const double p3 = std::pow(std::clamp(double(pressure), 0.0, 1.0), 3.0);
        const uint16_t kb = uint16_t(std::min(65535.0, std::round(burnishStep * p3)));
        const uint16_t kd = uint16_t(std::min(65535.0, std::round(damageStep * p3)));
        dirty.unite(depositAt(x, y, pressure, tip, kStep, kb, kd));
    }
    m_substeps += uint64_t(steps);
    return dirty;
}

DirtyRect Pencil::depositAt(int64_t fx, int64_t fy, float pressure, const Tip& tip, uint16_t k, uint16_t kb,
                            uint16_t kd)
{
    const int cx = int(floorDiv(fx, kOne)), cy = int(floorDiv(fy, kOne));
    m_contact.find(m_paper, tip, m_medium, cx, cy, pressure);
    const bool erasing = m_medium.kind == Medium::Kind::Eraser;
    if (erasing)
        m_contact.applyErase(k);
    else
        m_contact.applyDeposit(k, m_medium.ceiling);
    if (!erasing && kb > 0)
        m_contact.applyBurnish(kb);
    if (m_medium.deformRate > 0)
        m_contact.applyDeform(m_medium.deformRate);
    if (erasing && kd > 0)
        m_contact.applyDamage(kd);

    // Escribir el depósito de vuelta en los tiles, solo en los tramos con contacto (así no
    // se crean tiles donde la punta no tocó; la goma tampoco los crea donde no hay grafito).
    const int w = tip.width(), h = tip.height();
    const int x0 = cx - tip.originX(), y0 = cy - tip.originY();
    const uint16_t* pen = m_contact.penetration();

    DirtyRect dirty;
    for (int r = 0; r < h; ++r) {
        const int gy = y0 + r;
        if (gy < 0 || gy >= m_paper.height())
            continue;
        int c = std::max(0, -x0);
        const int cEnd = std::min(w, m_paper.width() - x0);
        while (c < cEnd) {
            const int gx = x0 + c;
            const int lx = gx % kTileSize;
            const int run = std::min(cEnd - c, kTileSize - lx);
            const uint16_t* penRun = pen + size_t(r) * w + size_t(c);
            if (std::any_of(penRun, penRun + run, [](uint16_t v) { return v > 0; })) {
                const int tx = gx / kTileSize, ty = gy / kTileSize;
                const int index = ty * m_paper.tilesX() + tx;
                if (erasing && !m_paper.findDepositTile(tx, ty)) {
                    c += run;
                    continue;
                }
                if (!m_tileMarked[size_t(index)]) {
                    // Primera escritura del trazo en este tile: avisar antes de tocarlo.
                    if (m_observer)
                        m_observer(index, m_paper.findDepositTile(tx, ty));
                    m_tileMarked[size_t(index)] = true;
                    m_strokeTiles.push_back(index);
                }
                uint16_t* tile = m_paper.depositTile(tx, ty);
                const size_t in = size_t(gy % kTileSize) * kTileSize + size_t(lx), from = size_t(r) * w + size_t(c);
                for (int pl = 0; pl < kTilePlanes; ++pl)
                    std::memcpy(tile + size_t(pl) * kTileCells + in, m_contact.plane(pl) + from, size_t(run) * 2);
                dirty.unite({gx, gy, gx + run, gy + 1});
            }
            c += run;
        }
    }
    if (!erasing && wears())
        wearAt(tip, k); // al final: puede rearmar la punta
    return dirty;
}

} // namespace drymedia
