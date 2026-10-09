#pragma once

#include "SheetMapping.h"

#include <atomic>
#include <thread>

namespace drymedia {
class Paper;
}

namespace cartuchera {

class SampleQueue;
struct DisplayImage;

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
    // Ctrl+Z / Ctrl+Y: se atienden en el hilo de simulación (el único que escribe el papel).
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
    // Blandura de la HB (1..255), para calibrar en vivo.
    void setSoftness(int softness) { m_softness = softness; }
    int softness() const { return m_softness; }
    // Diámetro de la mina en centésimas de mm, para calibrar en vivo.
    void setLeadDiameter(int hundredthsMm) { m_diameter = hundredthsMm; }
    int leadDiameter() const { return m_diameter; }

    // Pinta toda la imagen (hoja y afuera). Llamar antes de start().
    void renderAll();

private:
    void run();
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
    std::atomic<int> m_softness;
    std::atomic<int> m_diameter;
};

} // namespace cartuchera
