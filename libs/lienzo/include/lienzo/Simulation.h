#pragma once

#include "lienzo/Media.h"
#include "lienzo/SheetMapping.h"
#include "lienzo/Tone.h"
#include "lienzo/ViewRotation.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>
#include <thread>

namespace drymedia {
class Paper;
}

namespace lienzo {

// La altitud que recibe el lápiz: con el costado apagado (HU-73) se ignora la inclinación y
// la punta es siempre la vertical.
inline float penAltitude(float tabletAltitude, bool tilt)
{
    return tilt ? tabletAltitude : 90.0f;
}

// Cuándo recalcular la luz de la textura (HU-75) al girar la vista: con la rueda (HU-74) el
// giro cambia muchas veces por segundo y cada recálculo repinta la hoja entera, así que la
// luz espera a que el giro se quede quieto kSettleMs. La intensidad cambia en el acto.
class LightSchedule {
public:
    static constexpr double kSettleMs = 200;
    // true si hay que recalcular ahora con (view, percent); nowMs: reloj monotónico.
    bool due(double view, int percent, double nowMs);

private:
    double m_view = 0;    // con qué giro e intensidad se calculó
    int m_percent = -1;   // -1: nunca
    double m_seenView = 0, m_seenAt = 0; // el último giro visto y desde cuándo
};

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
    // Desgaste (HU-62): cada dureza (índice de kGradeNames) guarda su punta durante la sesión.
    // Llamar setGrade antes de setLead al cambiar de mina. requestSharpen afila la activa.
    void setGrade(int grade) { m_grade = grade; }
    void requestSharpen()
    {
        m_sharpenRequested = true;
        wake();
    }
    int wearPercent() const { return m_wearPercent; }
    // Punta seca (HU-61, tecla E): en vez de la mina, un estilete que no deposita y hunde el
    // papel (líneas blancas). La goma sigue en el otro extremo.
    void setStylus(bool stylus) { m_stylus = stylus; }
    bool stylus() const { return m_stylus; }
    // Costado (HU-73, tecla I): si está apagado, la punta es siempre la vertical.
    void setTilt(bool tilt) { m_tilt = tilt; }
    bool tilt() const { return m_tilt; }

    // Pinta toda la imagen (hoja y afuera). Llamar antes de start().
    void renderAll();

    // Textura del papel (HU-75), de 0 a 100 %. La luz está fija al escritorio: viene de
    // arriba a la izquierda de la pantalla, así que al girar la vista cambia sobre la hoja.
    void setTexture(int percent)
    {
        m_texturePercent = std::clamp(percent, 0, 100);
        wake();
    }
    int texture() const { return m_texturePercent; }
    static constexpr double kLightDegrees = 225.0; // hacia arriba a la izquierda, en pantalla

    // Rotación de la vista (HU-40): las muestras se llevan a la imagen sin rotar.
    void setRotation(const ViewRotation& rotation)
    {
        {
            std::lock_guard lock(m_rotationMutex);
            m_rotation = rotation;
        }
        wake(); // la luz de la textura (HU-75) se recalcula con el giro
    }

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
    bool updateLight(); // true si cambió la luz de la textura (hay que repintar todo)
    void repaintAllInStrips();

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
    std::mutex m_rotationMutex;
    ViewRotation m_rotation;
    std::atomic<uint64_t> m_lead;
    std::atomic<uint64_t> m_eraser;
    std::atomic<bool> m_erasing{false};
    std::atomic<bool> m_tilt{true};
    std::atomic<bool> m_stylus{false};
    std::atomic<int> m_grade{kHbIndex};
    std::atomic<bool> m_sharpenRequested{false};
    std::atomic<int> m_wearPercent{0};
    PaperTexture m_texture;
    std::atomic<int> m_texturePercent{0};
    LightSchedule m_light;
};

} // namespace lienzo
