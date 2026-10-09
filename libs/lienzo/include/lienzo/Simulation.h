#pragma once

#include "lienzo/Media.h"
#include "lienzo/SheetMapping.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>
#include <thread>

namespace drymedia {
class Paper;
}

namespace lienzo {

class SampleQueue;
struct DisplayImage;
struct SessionTimings;

// Hilo de simulación: el único que escribe el papel. Toma las muestras de la cola apenas
// llegan, las pasa por el lápiz (drymedia::Pencil) y recalcula el tono de las zonas que
// cambiaron en la imagen de pantalla.
class Simulation {
public:
    Simulation(drymedia::Paper& paper, SampleQueue& queue, DisplayImage& image, const SheetMapping& mapping);
    ~Simulation();

    void start();
    void stop();

    void requestClear()
    {
        m_clearRequested = true;
        wake();
    }
    // Deshacer (Z) y rehacer (Ctrl+Y): se atienden en el hilo de simulación (el único que escribe el papel).
    void requestUndo()
    {
        ++m_undoRequests;
        wake();
    }
    void requestRedo()
    {
        ++m_redoRequests;
        wake();
    }
    static constexpr int kUndoLimit = 100; // decisión del 10 de octubre de 2026
    // Sin deshacer (Ejercicios) no se guardan copias de los tiles. Llamar antes de start().
    void setUndo(bool enabled) { m_undoEnabled = enabled; }
    // La mina activa (HU-57): cambia al elegir otra dureza o al calibrar en vivo. Si
    // cambia en medio de un trazo, el trazo se cierra y sigue como uno nuevo.
    void setLead(const Lead& lead) { m_lead = lead.pack(); }
    Lead lead() const { return Lead::unpack(m_lead); }
    // La goma (HU-58): se usa al dar vuelta el lápiz. erasing() dice si la última muestra
    // llegó del extremo goma (para el overlay y la calibración en vivo).
    void setEraser(const Eraser& eraser) { m_eraser = eraser.pack(); }
    Eraser eraser() const { return Eraser::unpack(m_eraser); }
    bool erasing() const { return m_erasing; }

    // Pinta toda la imagen (hoja y afuera). Llamar antes de start().
    void renderAll();

    // Capa de guías (HU-64): BGRA premultiplicado del tamaño de la imagen, o vacía para
    // sacarla. Se puede llamar en cualquier momento: el hilo de simulación la toma y repinta
    // todo una vez.
    void setGuides(std::vector<uint32_t> guides);

    // Mediciones de tiempo para diagnóstico (opcional; llamar antes de start()).
    void setTimings(SessionTimings* timings) { m_timings = timings; }
    // Grabación de las muestras tal como llegan, en CSV (diagnóstico; llamar antes de start()).
    void setRecording(std::FILE* file) { m_recording = file; }

private:
    void run();
    const uint32_t* guides() const
    {
        return m_guides.size() == size_t(m_mapping.clientWidth) * size_t(m_mapping.clientHeight) ? m_guides.data()
                                                                                                : nullptr;
    }
    void wake(); // despierta al hilo aunque no haya muestras

    drymedia::Paper& m_paper;
    SampleQueue& m_queue;
    DisplayImage& m_image;
    SheetMapping m_mapping;
    std::thread m_thread;
    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_clearRequested{false};
    std::atomic<int> m_undoRequests{0};
    std::atomic<int> m_redoRequests{0};
    SessionTimings* m_timings = nullptr;
    std::FILE* m_recording = nullptr;
    bool m_undoEnabled = true;
    std::vector<uint32_t> m_guides; // la que usa el hilo de simulación
    std::mutex m_guidesMutex;
    std::vector<uint32_t> m_pendingGuides; // la que llega de afuera
    std::atomic<bool> m_guidesChanged{false};
    std::atomic<uint64_t> m_lead;
    std::atomic<uint64_t> m_eraser;
    std::atomic<bool> m_erasing{false};
};

} // namespace lienzo
