#pragma once

// Header privado de paintcore (ver Brush.h).

#include <QImage>
#include <QRect>
#include <QSize>

#include <cstdint>
#include <memory>
#include <vector>

struct MyPaintSurface2;

namespace paintcore::detail {

// Superficie de tiles de libmypaint (MyPaintTiledSurface2) para un lienzo de size
// píxeles, pintada a scale veces esa resolución (supersampling): libmypaint trabaja
// en coordenadas de superficie (las del lienzo × scale). Los tiles se guardan en el
// formato nativo de libmypaint (RGBA de 16 bits, premultiplicado, 1.0 == 1 << 15) y
// cada tile modificado se reduce a un QImage RGBA8 premultiplicado del tamaño del
// lienzo, promediando cada bloque de scale × scale: el grano y los bordes de los
// dabs quedan más finos que un píxel de pantalla. Fuera del lienzo los dabs se
// descartan.
class Surface {
public:
    // scale divide al tamaño de tile (64): 1, 2 o 4.
    explicit Surface(QSize size, int scale = 1);
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    MyPaintSurface2* handle() const;
    // Tamaño del lienzo (el de image()), no el de la superficie.
    QSize size() const { return m_image.size(); }
    int scale() const { return m_scale; }
    const QImage& image() const { return m_image; }

    // Envuelven los trazos: endAtomic() procesa los dabs pendientes y devuelve
    // el rectángulo modificado en coordenadas del lienzo (vacío si no cambió nada).
    void beginAtomic();
    QRect endAtomic();

    // Deja la superficie totalmente transparente.
    void clear();

private:
    struct Native;
    friend struct Native;

    uint16_t* tileBuffer(int tx, int ty);
    void copyTileToImage(int tx, int ty);

    std::unique_ptr<Native> m_native;
    int m_scale = 1;
    int m_tilesX = 0;
    int m_tilesY = 0;
    std::vector<uint16_t> m_tiles;       // m_tilesX * m_tilesY tiles contiguos
    std::vector<uint16_t> m_scratchTile; // destino de los dabs fuera del lienzo
    QImage m_image;
};

} // namespace paintcore::detail
