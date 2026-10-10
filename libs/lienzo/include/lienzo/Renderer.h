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

    // Pila de hojas (HU-83). Cada hoja que deja de ser la activa llega en la imagen de
    // pantalla (DisplayImage::retired) y queda en una textura de la GPU, por id. La hoja que
    // se muestra es la activa (shown = 0) u otra por id (el flip, HU-90); `below` son las de
    // abajo, teñidas (la mesa de luz). Sin otra hoja ni capas, el render es el de siempre.
    void setSheetRect(const RECT& sheet)
    {
        std::lock_guard lock(m_layersMutex);
        m_sheetRect = sheet;
    }
    void setLayers(uint64_t shown, std::vector<SheetLayer> below)
    {
        std::lock_guard lock(m_layersMutex);
        m_shown = shown;
        m_below = std::move(below);
    }
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
    std::string m_summary;
    std::mutex m_rotationMutex;
    ViewRotation m_rotation;
    std::mutex m_layersMutex;
    RECT m_sheetRect{};
    uint64_t m_shown = 0;
    std::vector<SheetLayer> m_below;
    std::vector<uint64_t> m_dropped;
};

} // namespace lienzo
