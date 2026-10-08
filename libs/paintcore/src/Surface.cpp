#include "Surface.h"

#include <mypaint-tiled-surface.h>

#include <algorithm>
#include <cstring>

namespace paintcore::detail {

namespace {

constexpr int kTileSize = MYPAINT_TILE_SIZE;
constexpr int kTileValues = kTileSize * kTileSize * 4; // RGBA por píxel
constexpr uint32_t kOne = 1u << 15;                    // 1.0 en libmypaint

// 15 bits (0..1<<15) a 8 bits (0..255), con redondeo.
inline uint8_t to8(uint16_t v)
{
    return static_cast<uint8_t>((v * 255u + kOne / 2) >> 15);
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

Surface::Surface(QSize size)
    : m_native(std::make_unique<Native>())
    , m_tilesX((size.width() + kTileSize - 1) / kTileSize)
    , m_tilesY((size.height() + kTileSize - 1) / kTileSize)
    , m_tiles(static_cast<size_t>(m_tilesX) * m_tilesY * kTileValues, 0)
    , m_scratchTile(kTileValues, 0)
    , m_image(size, QImage::Format_RGBA8888_Premultiplied)
{
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
    return QRect(roi.x, roi.y, roi.width, roi.height).intersected(m_image.rect());
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

    const uint16_t* tile = tileBuffer(tx, ty);
    const int x0 = tx * kTileSize;
    const int y0 = ty * kTileSize;
    const int width = std::min(kTileSize, m_image.width() - x0);
    const int height = std::min(kTileSize, m_image.height() - y0);

    for (int y = 0; y < height; ++y) {
        const uint16_t* src = tile + static_cast<size_t>(y) * kTileSize * 4;
        uint8_t* dst = m_image.scanLine(y0 + y) + static_cast<size_t>(x0) * 4;
        for (int i = 0; i < width * 4; ++i)
            dst[i] = to8(src[i]);
    }
}

} // namespace paintcore::detail
