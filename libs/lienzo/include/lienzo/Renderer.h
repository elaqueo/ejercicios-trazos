#pragma once

#include "lienzo/ViewRotation.h"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace lienzo {

struct DisplayImage;

// Una hoja que se ve debajo de la que se muestra (mesa de luz, HU-83), por id (SheetOrder).
struct SheetLayer {
    uint64_t sheet = 0;
    uint32_t color = 0; // BGRA
    float opacity = 1;
};

// Qué se ve en la región de la hoja (HU-90). La imagen de pantalla es la de la hoja activa,
// pero el render la sube de a partes (al cambiar de hoja, unos 10 frames): mientras no esté
// entera, la hoja pedida se ve desde su textura. Sin GPU, para las pruebas.
class SheetSource {
public:
    enum Kind { Image, Texture, Blank };
    // Cada vez que el render toma la imagen: de qué hoja es y si le quedaba algo por subir
    // antes y después de este frame.
    void latched(uint64_t imageSheet, bool pendingBefore, bool pendingAfter)
    {
        if (pendingBefore && imageSheet != m_uploaded)
            m_uploaded = 0; // empezó a subir otra hoja
        if (!pendingAfter)
            m_uploaded = imageSheet;
        m_image = imageSheet;
    }
    // La hoja cuya imagen ya está entera en la GPU (0 = ninguna).
    uint64_t uploaded() const { return m_uploaded; }
    // shown: la hoja pedida (0 = la activa: lo que tenga la imagen); hasTexture: si esa hoja
    // tiene textura.
    Kind pick(uint64_t shown, bool hasTexture) const
    {
        if (!shown || shown == m_uploaded)
            return Image;
        if (hasTexture)
            return Texture;
        return shown == m_image ? Image : Blank; // a medio subir es mejor que en blanco
    }

private:
    uint64_t m_uploaded = 0, m_image = 0;
};

// Hilo de render (spike HU-44): swapchain D3D11 FLIP_DISCARD con latencia 1 y waitable,
// render justo a tiempo con adelanto adaptativo (LeadController). Cada frame sube a la GPU
// solo lo que cambió de la imagen de pantalla. Mide la latencia muestra → vsync con
// GetFrameStatistics y la muestra en un recuadro (F3).
class Renderer {
public:
    Renderer(HWND hwnd, DisplayImage& image);
    ~Renderer();

    void start();
    // Detiene el hilo. Hay que llamarla desde el hilo de la ventana: espera atendiendo
    // mensajes (un join() a secas se traba, porque Present puede necesitar a la ventana).
    void stop();

    void toggleOverlay() { m_overlay = !m_overlay; }
    void setOverlay(bool visible) { m_overlay = visible; }
    // Texto extra para el recuadro (por ejemplo, la blandura); se pide cada 250 ms.
    void setExtraInfo(std::function<std::wstring()> extra) { m_extra = std::move(extra); }
    // Rotación de la vista (HU-40): a 0° se copia la imagen tal cual; girada, la GPU la
    // dibuja como un rectángulo rotado (afuera, el color de fuera de la hoja).
    void setRotation(const ViewRotation& rotation)
    {
        std::lock_guard lock(m_rotationMutex);
        m_rotation = rotation;
    }
    // Mediciones de tiempo para diagnóstico (opcional; llamar antes de start()).
    void setTimings(struct SessionTimings* timings) { m_timings = timings; }
    // El color de la hoja (HU-97): la hoja en blanco y el tinte de la mesa de luz. Antes de start().
    void setPaperColor(uint32_t color) { m_paperColor = color; }

    // Pila de hojas (HU-83). Cada hoja que deja de ser la activa llega en la imagen de
    // pantalla (DisplayImage::retired) y queda en una textura de la GPU, por id. La hoja que
    // se muestra es la activa (shown = 0) u otra por id (el flip, HU-90; ver SheetSource);
    // `below` son las de abajo, teñidas (la mesa de luz). Sin otra hoja ni capas, el render es
    // el de siempre. untilActive: `shown` es la que va a ser la activa (al soltar el flip): se
    // ve desde su textura hasta que la imagen la tenga entera, y de ahí en más, la activa.
    void setSheetRect(const RECT& sheet)
    {
        std::lock_guard lock(m_layersMutex);
        m_sheetRect = sheet;
    }
    void setLayers(uint64_t shown, std::vector<SheetLayer> below, bool untilActive = false)
    {
        std::lock_guard lock(m_layersMutex);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (shown && shown != m_shown && !untilActive && !m_flipQpc) // un flip: se mide hasta el Present
            m_flipQpc = now.QuadPart;
        if (!below.empty() && m_below.empty() && !m_lightQpc) // la mesa de luz aparece (HU-91)
            m_lightQpc = now.QuadPart;
        m_shown = shown;
        m_untilActive = untilActive;
        m_below = std::move(below);
    }
    // Cada flip presentado (HU-90): ms desde setLayers hasta el Present. En el hilo de render;
    // llamar antes de start().
    void setOnFlip(std::function<void(double ms)> callback) { m_onFlip = std::move(callback); }
    // Cada vez que la mesa de luz aparece (HU-91, RNF-03: al soltar el flip o al prenderla), ms
    // desde setLayers hasta el Present. En el hilo de render; llamar antes de start().
    void setOnLightTable(std::function<void(double ms)> callback) { m_onLightTable = std::move(callback); }
    void dropSheet(uint64_t sheet)
    {
        std::lock_guard lock(m_layersMutex);
        m_dropped.push_back(sheet);
    }

    // Resumen de la sesión para el log: latencias, adelanto final, vsyncs perdidos.
    std::string summary() const;

private:
    struct Impl;
    void run();

    HWND m_hwnd;
    DisplayImage& m_image;
    std::thread m_thread;
    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_overlay{false};
    std::function<std::wstring()> m_extra;
    struct SessionTimings* m_timings = nullptr;
    uint32_t m_paperColor = 0xFFF5F0E6u; // kPaperColor
    std::string m_summary;
    std::mutex m_rotationMutex;
    ViewRotation m_rotation;
    std::mutex m_layersMutex;
    RECT m_sheetRect{};
    uint64_t m_shown = 0;
    bool m_untilActive = false;
    int64_t m_flipQpc = 0; // cuándo se pidió el flip que todavía no se presentó
    std::function<void(double)> m_onFlip;
    int64_t m_lightQpc = 0; // cuándo apareció la mesa de luz que todavía no se presentó
    std::function<void(double)> m_onLightTable;
    std::vector<SheetLayer> m_below;
    std::vector<uint64_t> m_dropped;
};

} // namespace lienzo
