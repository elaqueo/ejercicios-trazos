#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace lienzo {

// La imagen de pantalla de una hoja (solo la región de la hoja, BGRA) comprimida con zstd.
struct PackedImage {
    int width = 0, height = 0;
    std::vector<uint8_t> bytes;

    bool empty() const { return bytes.empty(); }
    static PackedImage pack(int width, int height, const uint32_t* pixels);
    // false si está vacía o dañada.
    bool unpack(std::vector<uint32_t>& pixels) const;
};

// Las imágenes de las hojas que no son la activa (HU-85), comprimidas, para guardarlas en la
// toma sin redibujarlas (redibujar una hoja tarda ~108 ms; abrir la toma usa las guardadas).
// La simulación deja acá la imagen de cada hoja que sale, la misma que va al render, y un
// hilo propio la comprime (~8 ms) sin frenar el cambio de hoja. Seguro entre hilos.
class SheetImages {
public:
    SheetImages();
    ~SheetImages();
    SheetImages(const SheetImages&) = delete;
    SheetImages& operator=(const SheetImages&) = delete;

    void put(uint64_t sheet, int width, int height, std::vector<uint32_t> pixels); // la comprime en su hilo
    void putPacked(uint64_t sheet, PackedImage image);                             // ya comprimida
    // Espera a que termine de comprimir lo pendiente; vacía si la hoja no tiene imagen.
    PackedImage get(uint64_t sheet);
    void clear();

private:
    struct Raw {
        uint64_t sheet = 0;
        int width = 0, height = 0;
        std::vector<uint32_t> pixels;
    };
    void run();

    std::mutex m_mutex;
    std::condition_variable m_wake, m_done;
    std::deque<Raw> m_pending;
    bool m_busy = false, m_quit = false;
    std::map<uint64_t, PackedImage> m_images;
    std::thread m_thread;
};

} // namespace lienzo
