#include "drymedia/Contact.h"

#include "drymedia/Paper.h"
#include "kernel.h"

#include <intrin.h>

#include <algorithm>
#include <cstring>

namespace drymedia {

namespace {

constexpr int kSearchSteps = 16; // búsqueda binaria sobre D (u16)

const kernel::Impl& impl(Contact::Path path)
{
    return path == Contact::Path::Avx2 ? kernel::kAvx2 : kernel::kScalar;
}

} // namespace

bool Contact::avx2Available()
{
    int r[4];
    __cpuid(r, 1);
    const bool osxsave = (r[2] & (1 << 27)) != 0, avx = (r[2] & (1 << 28)) != 0;
    if (!osxsave || !avx || (_xgetbv(0) & 0x6) != 0x6)
        return false;
    __cpuidex(r, 7, 0);
    return (r[1] & (1 << 5)) != 0;
}

Contact::Contact(Path path)
    : m_path(path == Path::Scalar || !avx2Available() ? Path::Scalar : Path::Avx2)
    , m_cells(Tip::kTipCells)
    , m_relief(size_t(Tip::kTipCells))
    , m_crest(size_t(Tip::kTipCells))
    , m_deposit(size_t(Tip::kTipCells))
    , m_burnish(size_t(Tip::kTipCells))
    , m_deform(size_t(Tip::kTipCells))
    , m_damage(size_t(Tip::kTipCells))
    , m_surface(size_t(Tip::kTipCells))
    , m_penetration(size_t(Tip::kTipCells))
{
}

uint16_t Contact::find(const Paper& paper, const Tip& tip, const Medium& medium, int x, int y, float pressure)
{
    // 1. Copiar la huella del papel (relieve y los planos de los tiles) a arreglos contiguos. Fuera de la
    //    hoja, superficie 0: ahí la punta nunca toca.
    const int w = tip.width(), h = tip.height();
    const int x0 = x - tip.originX(), y0 = y - tip.originY();
    if (m_cells != tip.cells()) {
        m_cells = tip.cells();
        for (auto* v : {&m_relief, &m_crest, &m_deposit, &m_burnish, &m_deform, &m_damage, &m_surface, &m_penetration})
            v->resize(size_t(m_cells));
    }
    bool anyOutside = false;
    for (int r = 0; r < h; ++r) {
        const int gy = y0 + r;
        uint16_t* relief = m_relief.data() + size_t(r) * w;
        uint16_t* crest = m_crest.data() + size_t(r) * w;
        uint16_t* const planes[kTilePlanes] = {m_deposit.data() + size_t(r) * w, m_burnish.data() + size_t(r) * w,
                                               m_deform.data() + size_t(r) * w, m_damage.data() + size_t(r) * w};
        int c = 0;
        while (c < w) {
            const int gx = x0 + c;
            if (gy < 0 || gy >= paper.height() || gx < 0 || gx >= paper.width()) {
                relief[c] = 0;
                crest[c] = 0;
                for (uint16_t* plane : planes)
                    plane[c] = 0;
                anyOutside = true;
                ++c;
                continue;
            }
            const int lx = gx % kTileSize;
            const int run = std::min({w - c, kTileSize - lx, paper.width() - gx});
            const size_t at = size_t(gy) * size_t(paper.width()) + size_t(gx);
            std::memcpy(relief + c, paper.relief() + at, size_t(run) * 2);
            std::memcpy(crest + c, paper.crest() + at, size_t(run) * 2);
            const uint16_t* tile = paper.findDepositTile(gx / kTileSize, gy / kTileSize);
            const size_t in = size_t(gy % kTileSize) * kTileSize + size_t(lx);
            for (int pl = 0; pl < kTilePlanes; ++pl) {
                if (tile)
                    std::memcpy(planes[pl] + c, tile + size_t(pl) * kTileCells + in, size_t(run) * 2);
                else
                    std::memset(planes[pl] + c, 0, size_t(run) * 2);
            }
            c += run;
        }
    }

    const kernel::Impl& k = impl(m_path);
    const int shift = std::clamp(medium.reliefShift, 0, 15);
    k.surface(m_surface.data(), m_relief.data(), m_crest.data(), m_deposit.data(), m_burnish.data(), m_deform.data(),
              m_damage.data(), m_cells, kBase, shift);
    if (anyOutside) {
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c) {
                const int gx = x0 + c, gy = y0 + r;
                if (gy < 0 || gy >= paper.height() || gx < 0 || gx >= paper.width())
                    m_surface[size_t(r) * w + size_t(c)] = 0;
            }
    }

    // 2. Mayor D (punta más alta) con fuerza ≥ objetivo: la profundidad de contacto.
    const float p = std::clamp(pressure, 0.0f, 1.0f);
    const uint32_t target = uint32_t(double(p) * double(medium.forceScale) + 0.5);
    uint32_t lo = 0, hi = 65535;
    for (int it = 0; it < kSearchSteps; ++it) {
        const uint32_t mid = (lo + hi + 1) / 2;
        if (k.force(m_surface.data(), tip.heights(), m_cells, uint16_t(mid)) >= target)
            lo = mid;
        else
            hi = mid - 1;
    }
    const uint16_t depth = uint16_t(lo);
    k.penetration(m_penetration.data(), m_surface.data(), tip.heights(), m_cells, depth);
    m_force = k.force(m_surface.data(), tip.heights(), m_cells, depth);
    return depth;
}

void Contact::applyDeposit(uint16_t k, uint16_t ceiling)
{
    impl(m_path).deposit(m_deposit.data(), m_burnish.data(), m_damage.data(), m_penetration.data(), m_cells, k,
                         ceiling);
}

void Contact::applyErase(uint16_t k)
{
    impl(m_path).erase(m_deposit.data(), m_burnish.data(), m_penetration.data(), m_cells, k);
}

uint32_t Contact::meanPenetration() const
{
    uint32_t cells = 0;
    const uint32_t sum = impl(m_path).contactDeposit(m_penetration.data(), m_penetration.data(), m_cells, &cells);
    return cells ? sum / cells : 0;
}

void Contact::applyDeform(uint16_t rate)
{
    // El papel cede según la presión de contacto (la penetración media: la fuerza repartida en
    // el área que toca), no según el pico de una celda: el vértice del cono es fino y cualquier
    // mina lo pasaría. Las minas escalan la fuerza con su área, así que apoyan todas igual; la
    // punta seca concentra la misma mano en mucho menos papel.
    const uint32_t mean = meanPenetration();
    if (mean <= kYield)
        return;
    const uint16_t k = uint16_t(std::min<uint32_t>((uint32_t(rate) * (mean - kYield)) / kYield, 65535));
    impl(m_path).grow(m_deform.data(), m_penetration.data(), m_cells, 0, k, kMaxDeform);
}

void Contact::applyDamage(uint16_t kd)
{
    impl(m_path).grow(m_damage.data(), m_penetration.data(), m_cells, 0, kd, 65535);
}

void Contact::applyBurnish(uint16_t kb)
{
    const kernel::Impl& k = impl(m_path);
    uint32_t cells = 0;
    const uint32_t sum = k.contactDeposit(m_deposit.data(), m_penetration.data(), m_cells, &cells);
    if (cells == 0)
        return;
    // Hacia el promedio de las celdas en contacto. Sin el techo de la mina: arrastra grafito
    // que ya estaba (la 2H bruñe y empareja una capa de 4B más oscura que su techo).
    k.burnish(m_deposit.data(), m_burnish.data(), m_penetration.data(), m_cells, kb, uint16_t(sum / cells));
}

int Contact::cellsInContact() const
{
    return int(std::count_if(m_penetration.begin(), m_penetration.end(), [](uint16_t v) { return v > 0; }));
}

} // namespace drymedia
