#include "drymedia/Paper.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

namespace drymedia {

namespace {

// Tiles que reserva el pool de una vez (256 × 8 KB = 2 MB).
constexpr int kPoolBlock = 64; // 2 MB por bloque con 4 planos: llenar con ceros 8 MB de golpe frenaba la simulación (HU-76)
// Separación de la grilla de la ondulación, en celdas a 600 dpi (0,34 mm; a otra resolución
// se escala para que conserve su tamaño físico, HU-112).
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

constexpr uint32_t kGrainMax = 2047;

// Relieve en (x, y): grano fino por celda (0..depth, a lo sumo kGrainMax) + ondulación suave interpolada
// en una grilla de `k` celdas (0..2047). Solo enteros: igual en cualquier máquina. `crest`
// es el nivel de las crestas de la zona (la ondulación + el grano máximo): hasta ahí llena
// el grafito un valle (Contact).
uint16_t reliefValue(int x, int y, uint32_t seed, uint32_t k, uint32_t depth, uint16_t& crest)
{
    const uint32_t grain = (cellHash(uint32_t(x), uint32_t(y), seed) >> 21) * depth / kGrainMax; // depth = kGrainMax: tal cual
    const uint32_t gx = uint32_t(x) / k, gy = uint32_t(y) / k;
    const uint32_t fx = uint32_t(x) % k, fy = uint32_t(y) % k;
    const uint32_t waveSeed = seed ^ 0x9e3779b9U;
    const uint32_t c00 = cellHash(gx, gy, waveSeed) >> 21, c10 = cellHash(gx + 1, gy, waveSeed) >> 21;
    const uint32_t c01 = cellHash(gx, gy + 1, waveSeed) >> 21, c11 = cellHash(gx + 1, gy + 1, waveSeed) >> 21;
    const uint32_t wave = (c00 * (k - fx) * (k - fy) + c10 * fx * (k - fy) + c01 * (k - fx) * fy + c11 * fx * fy) / (k * k);
    crest = uint16_t(wave + depth);
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
    std::vector<uint16_t> relief, crest;
    uint64_t reliefHash = 0;

    TileSet set; // el depósito de la hoja con la que trabaja el papel (HU-82)

    void generateRelief()
    {
        relief.resize(size_t(width) * size_t(height));
        crest.resize(relief.size());
        const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
        const uint32_t wave = uint32_t(std::max(1, kWaveCell * spec.dpi / 600));
        const uint32_t depth = uint32_t(std::clamp(spec.grain, 0, int(kGrainMax)));
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < threads; ++t) {
            pool.emplace_back([this, t, threads, wave, depth] {
                for (int y = int(t); y < height; y += int(threads)) {
                    uint16_t* row = relief.data() + size_t(y) * size_t(width);
                    uint16_t* crestRow = crest.data() + size_t(y) * size_t(width);
                    for (int x = 0; x < width; ++x)
                        row[x] = reliefValue(x, y, spec.seed, wave, depth, crestRow[x]);
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
    static void reserveBlock(TileSet& set)
    {
        set.m_blocks.push_back(std::make_unique<uint16_t[]>(size_t(kPoolBlock) * kTileStride));
        set.m_nextFree = set.m_blocks.back().get();
        set.m_freeInBlock = kPoolBlock;
    }

    static uint16_t* takeTile(TileSet& set)
    {
        if (!set.m_recycled.empty()) {
            uint16_t* tile = set.m_recycled.back();
            set.m_recycled.pop_back();
            return tile;
        }
        if (set.m_freeInBlock == 0)
            reserveBlock(set); // una vez cada kPoolBlock tiles
        uint16_t* tile = set.m_nextFree;
        set.m_nextFree += kTileStride;
        --set.m_freeInBlock;
        return tile;
    }

    // Los planos que guarda un tile empaquetado: el depósito y los que marca `planes`.
    static int packedPlanes(uint8_t planes)
    {
        int n = 1;
        for (int pl = 1; pl < kTilePlanes; ++pl)
            n += (planes >> pl) & 1;
        return n;
    }

    // HU-112: deja en el conjunto solo los planos que usa cada tile y suelta el pool.
    static void pack(TileSet& set)
    {
        if (set.m_isPacked)
            return;
        size_t cells = 0;
        for (size_t i = 0; i < set.m_tiles.size(); ++i)
            if (set.m_tiles[i])
                cells += size_t(packedPlanes(set.m_planes[i])) * kTileCells;
        std::vector<uint16_t> packed(cells);
        uint16_t* out = packed.data();
        for (size_t i = 0; i < set.m_tiles.size(); ++i) {
            const uint16_t* tile = set.m_tiles[i];
            if (!tile)
                continue;
            set.m_tiles[i] = out;
            for (int pl = 0; pl < kTilePlanes; ++pl)
                if (pl == kDepositPlane || ((set.m_planes[i] >> pl) & 1)) {
                    out = std::copy_n(tile + size_t(pl) * kTileCells, kTileCells, out);
                }
        }
        set.m_packed = std::move(packed);
        set.m_blocks.clear();
        set.m_recycled.clear();
        set.m_nextFree = nullptr;
        set.m_freeInBlock = 0;
        set.m_isPacked = true;
    }

    // Al revés: cada tile vuelve a tener sus kTilePlanes planos, en un pool nuevo.
    static void unpack(TileSet& set)
    {
        if (!set.m_isPacked)
            return;
        for (size_t i = 0; i < set.m_tiles.size(); ++i) {
            const uint16_t* in = set.m_tiles[i];
            if (!in)
                continue;
            uint16_t* tile = takeTile(set);
            for (int pl = 0; pl < kTilePlanes; ++pl)
                if (pl == kDepositPlane || ((set.m_planes[i] >> pl) & 1)) {
                    std::copy_n(in, kTileCells, tile + size_t(pl) * kTileCells);
                    in += kTileCells;
                }
            set.m_tiles[i] = tile;
        }
        set.m_packed.clear();
        set.m_packed.shrink_to_fit();
        set.m_isPacked = false;
    }

    void release(uint16_t*& slot)
    {
        std::fill(slot, slot + kTileStride, uint16_t(0));
        set.m_recycled.push_back(slot);
        slot = nullptr;
    }
};

Paper::Paper(const PaperSpec& spec)
    : d(std::make_unique<Impl>())
{
    d->spec = spec;
    d->width = int(std::lround(spec.widthMm * cellsPerMm()));
    d->height = int(std::lround(spec.heightMm * cellsPerMm()));
    d->tilesX = (d->width + kTileSize - 1) / kTileSize;
    d->tilesY = (d->height + kTileSize - 1) / kTileSize;
    d->set = newTileSet();
    d->generateRelief();
    Impl::reserveBlock(d->set); // el primer bloque, antes de que empiece el dibujo
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

const uint16_t* Paper::crest() const
{
    return d->crest.data();
}

uint16_t Paper::reliefAt(int x, int y) const
{
    return d->relief[size_t(y) * size_t(d->width) + size_t(x)];
}

uint16_t* Paper::depositTile(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return nullptr;
    uint16_t*& slot = d->set.m_tiles[size_t(ty) * size_t(d->tilesX) + size_t(tx)];
    if (!slot) {
        slot = Impl::takeTile(d->set);
        ++d->set.m_tileCount;
    }
    return slot;
}

const uint16_t* Paper::findDepositTile(int tx, int ty) const
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return nullptr;
    return d->set.m_tiles[size_t(ty) * size_t(d->tilesX) + size_t(tx)];
}

uint8_t Paper::tilePlanes(int tx, int ty) const
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return 0;
    return d->set.m_planes[size_t(ty) * size_t(d->tilesX) + size_t(tx)];
}

void Paper::markTilePlanes(int tx, int ty, uint8_t planes)
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return;
    d->set.m_planes[size_t(ty) * size_t(d->tilesX) + size_t(tx)] |= planes;
}

void Paper::setTilePlanes(int tx, int ty, uint8_t planes)
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return;
    d->set.m_planes[size_t(ty) * size_t(d->tilesX) + size_t(tx)] = planes;
}

void Paper::refreshTilePlanes(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return;
    const size_t i = size_t(ty) * size_t(d->tilesX) + size_t(tx);
    uint8_t used = 0;
    if (const uint16_t* tile = d->set.m_tiles[i])
        for (int pl = 1; pl < kTilePlanes; ++pl)
            if (std::any_of(tile + size_t(pl) * kTileCells, tile + size_t(pl + 1) * kTileCells,
                            [](uint16_t v) { return v != 0; }))
                used |= uint8_t(1 << pl);
    d->set.m_planes[i] = used;
}

void Paper::clear()
{
    std::fill(d->set.m_planes.begin(), d->set.m_planes.end(), uint8_t(0));
    for (uint16_t*& slot : d->set.m_tiles)
        if (slot)
            d->release(slot);
    d->set.m_tileCount = 0;
}

void Paper::releaseTile(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= d->tilesX || ty >= d->tilesY)
        return;
    uint16_t*& slot = d->set.m_tiles[size_t(ty) * size_t(d->tilesX) + size_t(tx)];
    if (!slot)
        return;
    d->release(slot);
    d->set.m_planes[size_t(ty) * size_t(d->tilesX) + size_t(tx)] = 0;
    --d->set.m_tileCount;
}

size_t Paper::tileCount() const
{
    return d->set.m_tileCount;
}

TileSet Paper::newTileSet() const
{
    TileSet set;
    set.m_tiles.assign(size_t(d->tilesX) * size_t(d->tilesY), nullptr);
    set.m_planes.assign(set.m_tiles.size(), 0);
    return set;
}

TileSet Paper::copyTiles() const
{
    TileSet copy = newTileSet();
    for (size_t i = 0; i < d->set.m_tiles.size(); ++i) {
        if (!d->set.m_tiles[i])
            continue;
        uint16_t* tile = Impl::takeTile(copy);
        std::copy_n(d->set.m_tiles[i], kTileStride, tile);
        copy.m_tiles[i] = tile;
    }
    copy.m_planes = d->set.m_planes;
    copy.m_tileCount = d->set.m_tileCount;
    return copy;
}

std::vector<uint8_t> Paper::serializeTiles(const TileSet* set, const std::function<bool()>& keepGoing) const
{
    const TileSet& s = set ? *set : d->set;
    std::vector<uint8_t> out;
    if (s.m_tiles.size() != d->set.m_tiles.size()) { // vacío o de otro tamaño: hoja en blanco
        const TileSet blank = newTileSet();
        return serializeTiles(&blank);
    }
    std::vector<uint32_t> index;
    size_t cells = 0;
    for (size_t i = 0; i < s.m_tiles.size(); ++i)
        if (s.m_tiles[i]) {
            index.push_back(uint32_t(i));
            cells += size_t(Impl::packedPlanes(s.m_planes[i])) * kTileCells;
        }
    const uint32_t head[3] = {uint32_t(d->tilesX), uint32_t(d->tilesY), uint32_t(index.size())};
    // Se agrega de a partes sobre la memoria reservada, sin llenarla antes de ceros: así cada
    // tramo entre consultas a keepGoing es corto (~0,2 ms).
    out.reserve(sizeof(head) + index.size() * 5 + cells * sizeof(uint16_t));
    const auto append = [&out](const void* data, size_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        out.insert(out.end(), bytes, bytes + size);
    };
    append(head, sizeof(head));
    append(index.data(), index.size() * 4);
    for (const uint32_t i : index)
        out.push_back(s.m_planes[i]);
    constexpr size_t kTilesPerCheck = 128;
    for (size_t n = 0; n < index.size(); ++n) {
        if (keepGoing && n % kTilesPerCheck == 0 && !keepGoing())
            return {};
        const uint32_t i = index[n];
        const uint16_t* tile = s.m_tiles[i];
        const uint8_t planes = s.m_planes[i];
        for (int pl = 0; pl < kTilePlanes; ++pl) {
            if (pl != kDepositPlane && !((planes >> pl) & 1))
                continue;
            // Empaquetado: los planos que usa, seguidos; si no, cada uno en su lugar del tile.
            const uint16_t* plane = s.m_isPacked ? tile : tile + size_t(pl) * kTileCells;
            append(plane, kTileCells * sizeof(uint16_t));
            if (s.m_isPacked)
                tile += kTileCells;
        }
    }
    return out;
}

bool Paper::deserializeTiles(const uint8_t* data, size_t size, TileSet& out) const
{
    uint32_t head[3];
    if (size < sizeof(head))
        return false;
    std::memcpy(head, data, sizeof(head));
    const size_t count = head[2];
    if (head[0] != uint32_t(d->tilesX) || head[1] != uint32_t(d->tilesY) || count > d->set.m_tiles.size() ||
        size < sizeof(head) + count * 5)
        return false;
    const uint8_t* r = data + sizeof(head);
    std::vector<uint32_t> index(count);
    std::memcpy(index.data(), r, count * 4);
    const uint8_t* planes = r + count * 4;
    size_t cells = 0;
    for (size_t k = 0; k < count; ++k) {
        if (index[k] >= d->set.m_tiles.size() || (k > 0 && index[k] <= index[k - 1]) || (planes[k] & ~0x1Eu))
            return false;
        cells += size_t(Impl::packedPlanes(planes[k])) * kTileCells;
    }
    const uint8_t* body = planes + count;
    if (size_t(data + size - body) != cells * sizeof(uint16_t))
        return false;
    TileSet set = newTileSet();
    set.m_packed.resize(cells);
    std::memcpy(set.m_packed.data(), body, cells * sizeof(uint16_t));
    uint16_t* tile = set.m_packed.data();
    for (size_t k = 0; k < count; ++k) {
        set.m_tiles[index[k]] = tile;
        set.m_planes[index[k]] = planes[k];
        tile += size_t(Impl::packedPlanes(planes[k])) * kTileCells;
    }
    set.m_tileCount = count;
    set.m_isPacked = true;
    out = std::move(set);
    return true;
}

void Paper::swapTiles(TileSet& other)
{
    if (other.m_tiles.size() != d->set.m_tiles.size())
        other = newTileSet(); // vacío o de otro tamaño: entra una hoja en blanco
    std::swap(d->set, other);
    Impl::unpack(d->set); // la que entra, con todos sus planos
    Impl::pack(other);    // la que sale, solo con los que usa (HU-112)
    if (d->set.m_recycled.empty() && d->set.m_freeInBlock == 0)
        Impl::reserveBlock(d->set);
}

size_t TileSet::bytes() const
{
    return m_blocks.size() * size_t(kPoolBlock) * kTileStride * sizeof(uint16_t) + m_packed.size() * sizeof(uint16_t) +
           (m_tiles.size() * sizeof(uint16_t*) + m_planes.size());
}

uint64_t Paper::reliefHash() const
{
    return d->reliefHash;
}

uint64_t Paper::hash() const
{
    uint64_t h = d->reliefHash;
    for (size_t i = 0; i < d->set.m_tiles.size(); ++i) {
        if (!d->set.m_tiles[i])
            continue;
        fnv(h, &i, sizeof(i));
        // El plano de color (HU-99) entra solo si el tile lo usa: así una hoja sin color da el
        // mismo hash que antes de que existiera (Cartuchera, Ejercicios).
        const bool color = (d->set.m_planes[i] >> kColorPlane) & 1;
        fnv(h, d->set.m_tiles[i], size_t(color ? kTilePlanes : kColorPlane) * kTileCells * sizeof(uint16_t));
    }
    return h;
}

} // namespace drymedia
