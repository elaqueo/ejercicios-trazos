#include "drymedia/Paper.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace drymedia {

namespace {

// Tiles que reserva el pool de una vez (256 × 8 KB = 2 MB).
constexpr int kPoolBlock = 256;
// Separación de la grilla de la ondulación, en celdas.
constexpr int kWaveCell = 8;

uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t cellHash(uint32_t x, uint32_t y, uint32_t seed)
{
    return hash32(x * 73856093U ^ y * 19349663U ^ hash32(seed));
}

// Relieve en (x, y): grano fino por celda (0..2047) + ondulación suave interpolada
// en una grilla de kWaveCell (0..2047). Solo enteros: igual en cualquier máquina.
uint16_t reliefValue(int x, int y, uint32_t seed)
{
    const uint32_t grain = cellHash(uint32_t(x), uint32_t(y), seed) >> 21;
    const uint32_t gx = uint32_t(x / kWaveCell), gy = uint32_t(y / kWaveCell);
    const uint32_t fx = uint32_t(x % kWaveCell), fy = uint32_t(y % kWaveCell);
    const uint32_t waveSeed = seed ^ 0x9e3779b9U;
    const uint32_t c00 = cellHash(gx, gy, waveSeed) >> 21, c10 = cellHash(gx + 1, gy, waveSeed) >> 21;
    const uint32_t c01 = cellHash(gx, gy + 1, waveSeed) >> 21, c11 = cellHash(gx + 1, gy + 1, waveSeed) >> 21;
    const uint32_t k = kWaveCell;
    const uint32_t wave = (c00 * (k - fx) * (k - fy) + c10 * fx * (k - fy) + c01 * (k - fx) * fy + c11 * fx * fy) / (k * k);
    return uint16_t(grain + wave);
}

void fnv(uint64_t& h, const void* data, size_t bytes)
{
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < bytes; ++i)
        h = (h ^ p[i]) * 1099511628211ULL;
}

} // namespace

struct Paper::Impl {
    PaperSpec spec;
    int width = 0, height = 0, tilesX = 0, tilesY = 0;
    std::vector<uint16_t> relief;
    uint64_t reliefHash = 0;

    std::vector<uint16_t*> tiles; // tilesX × tilesY; nullptr = no tocado
    std::vector<std::unique_ptr<uint16_t[]>> blocks;
    uint16_t* nextFree = nullptr;
    int freeInBlock = 0;
    std::vector<uint16_t*> recycled; // tiles devueltos por clear(), ya en cero
    size_t tileCount = 0;

    void generateRelief()
    {
        relief.resize(size_t(width) * size_t(height));
        const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < threads; ++t) {
            pool.emplace_back([this, t, threads] {
                for (int y = int(t); y < height; y += int(threads)) {
                    uint16_t* row = relief.data() + size_t(y) * size_t(width);
                    for (int x = 0; x < width; ++x)
                        row[x] = reliefValue(x, y, spec.seed);
                }
            });
        }
        for (std::thread& th : pool)
            th.join();

        reliefHash = 1469598103934665603ULL;
        fnv(reliefHash, &width, sizeof(width));
        fnv(reliefHash, &height, sizeof(height));
        fnv(reliefHash, relief.data(), relief.size() * sizeof(uint16_t));
    }

    // Bloque nuevo de tiles, ya en cero.
    void reserveBlock()
    {
        blocks.push_back(std::make_unique<uint16_t[]>(size_t(kPoolBlock) * kTileCells));
        nextFree = blocks.back().get();
        freeInBlock = kPoolBlock;
    }

    uint16_t* takeTile()
    {
        if (!recycled.empty()) {
            uint16_t* tile = recycled.back();
            recycled.pop_back();
            return tile;
        }
        if (freeInBlock == 0)
            reserveBlock(); // una vez cada kPoolBlock tiles
        uint16_t* tile = nextFree;
        nextFree += kTileCells;
        --freeInBlock;
        return tile;
    }
};

Paper::Paper(const PaperSpec& spec)
    : d(std::make_unique<Impl>())
{
    d->spec = spec;
    d->width = int(std::lround(spec.widthMm * kCellsPerMm));
    d->height = int(std::lround(spec.heightMm * kCellsPerMm));
    d->tilesX = (d->width + kTileSize - 1) / kTileSize;
    d->tilesY = (d->height + kTileSize - 1) / kTileSize;
    d->tiles.assign(size_t(d->tilesX) * size_t(d->tilesY), nullptr);
    d->generateRelief();
    d->reserveBlock(); // el primer bloque, antes de que empiece el dibujo
}

Paper::~Paper() = default;

const PaperSpec& Paper::spec() const
{
    return d->spec;
}

int Paper::width() const
{
    return d->width;
}

int Paper::height() const
{
    return d->height;
}

int Paper::tilesX() const
{
    return d->tilesX;
}

int Paper::tilesY() const
{
    return d->tilesY;
}

const uint16_t* Paper::relief() const
{
    return d->relief.data();
}

uint16_t Paper::reliefAt(int x, int y) const
{
    return d->relief[size_t(y) * size_t(d->width) + size_t(x)];
}

uint16_t* Paper::depositTile(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return nullptr;
    uint16_t*& slot = d->tiles[size_t(ty) * size_t(d->tilesX) + size_t(tx)];
    if (!slot) {
        slot = d->takeTile();
        ++d->tileCount;
    }
    return slot;
}

const uint16_t* Paper::findDepositTile(int tx, int ty) const
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return nullptr;
    return d->tiles[size_t(ty) * size_t(d->tilesX) + size_t(tx)];
}

void Paper::clear()
{
    for (uint16_t*& slot : d->tiles) {
        if (!slot)
            continue;
        std::fill(slot, slot + kTileCells, uint16_t(0));
        d->recycled.push_back(slot);
        slot = nullptr;
    }
    d->tileCount = 0;
}

size_t Paper::tileCount() const
{
    return d->tileCount;
}

uint64_t Paper::reliefHash() const
{
    return d->reliefHash;
}

uint64_t Paper::hash() const
{
    uint64_t h = d->reliefHash;
    for (size_t i = 0; i < d->tiles.size(); ++i) {
        if (!d->tiles[i])
            continue;
        fnv(h, &i, sizeof(i));
        fnv(h, d->tiles[i], size_t(kTileCells) * sizeof(uint16_t));
    }
    return h;
}

} // namespace drymedia
