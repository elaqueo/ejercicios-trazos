#include "Surface.h"

#include <mypaint-tiled-surface.h>

#include <algorithm>
#include <cstring>

namespace paintcore::detail {

namespace {

constexpr int kTileSize = MYPAINT_TILE_SIZE;
constexpr int kTileValues = kTileSize * kTileSize * 4; // RGBA por píxel
constexpr uint32_t kOne = 1u << 15;                    // 1.0 en libmypaint

// Suma de count valores de 15 bits (0..1<<15) a su promedio en 8 bits (0..255), con
// redondeo.
inline uint8_t to8(uint32_t sum, uint32_t count)
{
    const uint32_t den = count * kOne;
    return static_cast<uint8_t>((sum * 255u + den / 2) / den);
}

} // namespace

// Struct "derivado" de MyPaintTiledSurface2 al estilo C: el padre va primero,
// así libmypaint puede castear el puntero, y los callbacks recuperan el Surface.
struct Surface::Native {
    MyPaintTiledSurface2 tiled;
    Surface* owner;

    static void tileRequestStart(MyPaintTiledSurface2* self, MyPaintTileRequest* request)
    {
        Surface* owner = reinterpret_cast<Native*>(self)->owner;
        request->buffer = owner->tileBuffer(request->tx, request->ty);
    }

    static void tileRequestEnd(MyPaintTiledSurface2* self, MyPaintTileRequest* request)
    {
        Surface* owner = reinterpret_cast<Native*>(self)->owner;
        if (!request->readonly)
            owner->copyTileToImage(request->tx, request->ty);
    }
};

Surface::Surface(QSize size, int scale)
    : m_native(std::make_unique<Native>())
    , m_scale(scale)
    , m_tilesX((size.width() * scale + kTileSize - 1) / kTileSize)
    , m_tilesY((size.height() * scale + kTileSize - 1) / kTileSize)
    , m_tiles(static_cast<size_t>(m_tilesX) * m_tilesY * kTileValues, 0)
    , m_scratchTile(kTileValues, 0)
    , m_image(size, QImage::Format_RGBA8888_Premultiplied)
{
    Q_ASSERT(scale >= 1 && kTileSize % scale == 0);
    m_image.fill(Qt::transparent);
    m_native->owner = this;
    mypaint_tiled_surface2_init(&m_native->tiled, &Native::tileRequestStart, &Native::tileRequestEnd);
}

Surface::~Surface()
{
    mypaint_tiled_surface2_destroy(&m_native->tiled);
}

MyPaintSurface2* Surface::handle() const
{
    return &m_native->tiled.parent;
}

void Surface::beginAtomic()
{
    mypaint_tiled_surface2_begin_atomic(&m_native->tiled);
}

QRect Surface::endAtomic()
{
    MyPaintRectangle roi{};
    MyPaintRectangles rois{1, &roi};
    mypaint_tiled_surface2_end_atomic(&m_native->tiled, &rois);
    if (roi.width <= 0 || roi.height <= 0)
        return {};
    // De la superficie al lienzo, redondeando hacia afuera.
    const auto down = [this](int v) { return v >= 0 ? v / m_scale : -((-v + m_scale - 1) / m_scale); };
    const QRect rect(QPoint(down(roi.x), down(roi.y)),
                     QPoint(down(roi.x + roi.width - 1), down(roi.y + roi.height - 1)));
    return rect.intersected(m_image.rect());
}

void Surface::clear()
{
    std::fill(m_tiles.begin(), m_tiles.end(), uint16_t{0});
    m_image.fill(Qt::transparent);
}

uint16_t* Surface::tileBuffer(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= m_tilesX || ty >= m_tilesY) {
        // Fuera del lienzo: libmypaint lee y escribe acá; se limpia en cada pedido
        // para que nunca "aparezca" color al leer.
        std::fill(m_scratchTile.begin(), m_scratchTile.end(), uint16_t{0});
        return m_scratchTile.data();
    }
    return m_tiles.data() + (static_cast<size_t>(ty) * m_tilesX + tx) * kTileValues;
}

void Surface::copyTileToImage(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= m_tilesX || ty >= m_tilesY)
        return;

    // El tile cubre un bloque de kTileSize / scale píxeles del lienzo; cada píxel es el
    // promedio de scale × scale píxeles de la superficie (premultiplicados, así que
    // promediar no deja halos).
    const uint16_t* tile = tileBuffer(tx, ty);
    const int s = m_scale;
    const int block = kTileSize / s;
    const int x0 = tx * block;
    const int y0 = ty * block;
    const int width = std::min(block, m_image.width() - x0);
    const int height = std::min(block, m_image.height() - y0);
    const uint32_t count = static_cast<uint32_t>(s * s);

    for (int y = 0; y < height; ++y) {
        uint8_t* dst = m_image.scanLine(y0 + y) + static_cast<size_t>(x0) * 4;
        for (int x = 0; x < width; ++x) {
            uint32_t sum[4] = {};
            for (int sy = 0; sy < s; ++sy) {
                const uint16_t* src = tile + (static_cast<size_t>(y * s + sy) * kTileSize + x * s) * 4;
                for (int sx = 0; sx < s; ++sx)
                    for (int c = 0; c < 4; ++c)
                        sum[c] += src[sx * 4 + c];
            }
            for (int c = 0; c < 4; ++c)
                dst[x * 4 + c] = to8(sum[c], count);
        }
    }
}

} // namespace paintcore::detail
