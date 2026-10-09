#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace cartuchera {

struct DisplayImage;

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
    // Mediciones de tiempo para diagnóstico (opcional; llamar antes de start()).
    void setTimings(struct SessionTimings* timings) { m_timings = timings; }

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
};

} // namespace cartuchera
