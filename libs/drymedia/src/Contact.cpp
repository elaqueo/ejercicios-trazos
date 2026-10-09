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
    , m_deposit(size_t(Tip::kTipCells))
    , m_surface(size_t(Tip::kTipCells))
    , m_penetration(size_t(Tip::kTipCells))
{
}

uint16_t Contact::find(const Paper& paper, const Tip& tip, const Medium& medium, int x, int y, float pressure)
{
    // 1. Copiar la huella del papel (relieve y depósito) a arreglos contiguos. Fuera de la
    //    hoja, superficie 0: ahí la punta nunca toca.
    const int s = tip.size();
    const int x0 = x - tip.center(), y0 = y - tip.center();
    if (m_cells != tip.cells()) {
        m_cells = tip.cells();
        for (auto* v : {&m_relief, &m_deposit, &m_surface, &m_penetration})
            v->resize(size_t(m_cells));
    }
    bool anyOutside = false;
    for (int r = 0; r < s; ++r) {
        const int gy = y0 + r;
        uint16_t* relief = m_relief.data() + size_t(r) * s;
        uint16_t* deposit = m_deposit.data() + size_t(r) * s;
        int c = 0;
        while (c < s) {
            const int gx = x0 + c;
            if (gy < 0 || gy >= paper.height() || gx < 0 || gx >= paper.width()) {
                relief[c] = 0;
                deposit[c] = 0;
                anyOutside = true;
                ++c;
                continue;
            }
            const int lx = gx % kTileSize;
            const int run = std::min({s - c, kTileSize - lx, paper.width() - gx});
            std::memcpy(relief + c, paper.relief() + size_t(gy) * size_t(paper.width()) + size_t(gx), size_t(run) * 2);
            const uint16_t* tile = paper.findDepositTile(gx / kTileSize, gy / kTileSize);
            if (tile)
                std::memcpy(deposit + c, tile + size_t(gy % kTileSize) * kTileSize + size_t(lx), size_t(run) * 2);
            else
                std::memset(deposit + c, 0, size_t(run) * 2);
            c += run;
        }
    }

    const kernel::Impl& k = impl(m_path);
    k.surface(m_surface.data(), m_relief.data(), m_deposit.data(), m_cells, kBase, std::clamp(medium.reliefShift, 0, 15));
    if (anyOutside) {
        for (int r = 0; r < s; ++r)
            for (int c = 0; c < s; ++c) {
                const int gx = x0 + c, gy = y0 + r;
                if (gy < 0 || gy >= paper.height() || gx < 0 || gx >= paper.width())
                    m_surface[size_t(r) * s + size_t(c)] = 0;
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
    impl(m_path).deposit(m_deposit.data(), m_penetration.data(), m_cells, k, ceiling);
}

void Contact::applyErase(uint16_t k)
{
    impl(m_path).erase(m_deposit.data(), m_penetration.data(), m_cells, k);
}

int Contact::cellsInContact() const
{
    return int(std::count_if(m_penetration.begin(), m_penetration.end(), [](uint16_t v) { return v > 0; }));
}

} // namespace drymedia
