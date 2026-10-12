#pragma once

#include "lienzo/Media.h"
#include "lienzo/SheetStack.h"
#include "lienzo/SheetMapping.h"
#include "lienzo/TakeSaver.h"
#include "lienzo/Tone.h"
#include "lienzo/ViewRotation.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
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
class SheetImages;
struct DisplayImage;
struct SessionTimings;

// Hilo de simulación: el único que escribe el papel. Toma las muestras de la cola apenas
// llegan, las pasa por el lápiz (drymedia::Pencil) y recalcula el tono de las zonas que
// cambiaron en la imagen de pantalla.
class Simulation {
public:
    // sheets: la pila de hojas (HU-82), del mismo papel; la simulación es la única que la cambia.
    Simulation(drymedia::Paper& paper, SheetStack& sheets, SampleQueue& queue, DisplayImage& image,
               const SheetMapping& mapping);
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

    // Pila de hojas (HU-82): se atienden en el hilo de simulación, en orden, justo entre las
    // muestras que ya habían llegado y las que llegan después (HU-90; el trazo en curso se
    // cierra). Cambiar la hoja del papel
    // repinta la imagen entera. Insertar, duplicar, limpiar y quitar se deshacen con Z
    // (HU-87, si hay deshacer); mover también (HU-88).
    void requestAddSheet(int at) { pushSheetCommand({SheetCommand::Add, at, 0}); }
    void requestInsertSheet() { pushSheetCommand({SheetCommand::Insert, 0, 0}); }       // después de la activa, y activa
    void requestDuplicateSheet() { pushSheetCommand({SheetCommand::Duplicate, 0, 0}); } // ídem, una copia
    void requestClearSheet() { pushSheetCommand({SheetCommand::Clear, 0, 0}); }         // limpia la activa, con deshacer
    void requestRemoveSheet(int index) { pushSheetCommand({SheetCommand::Remove, index, 0}); }
    void requestMoveSheet(int from, int to) { pushSheetCommand({SheetCommand::Move, from, to}); }
    void requestActivateSheet(int index) { pushSheetCommand({SheetCommand::Activate, index, 0}); }
    // Lo ya atendido, para la interfaz (HU-87): el orden de la pila, cuántos tiles tiene la
    // activa (0 = en blanco) y cuántos pedidos de pila, deshacer y rehacer se atendieron.
    struct SheetState {
        SheetOrder order;
        size_t activeTiles = 0;
        uint64_t applied = 0;
    };
    SheetState sheetState() const
    {
        std::lock_guard lock(m_stateMutex);
        return m_state;
    }
    // Cada vez que se atendió algún pedido de pila, deshacer o rehacer (HU-91: la mesa de luz
    // se recalcula). En el hilo de simulación; llamar antes de start().
    void setOnSheetState(std::function<void()> callback) { m_onSheetState = std::move(callback); }
    int sheetCount() const { return sheetState().order.count; }
    int activeSheet() const { return sheetState().order.active; }
    double lastActivationMs() const { return m_activationMs; }
    // Perfil de medio (HU-112): con Animation, la mina y la goma sin bruñido, hundimiento,
    // daño de fibra ni desgaste. Llamar antes de start().
    void setProfile(MediumProfile profile) { m_profile = profile; }
    // El color de la hoja (HU-97). Llamar antes de renderAll() y start().
    void setPaperColor(uint32_t color) { m_paperColor = color; }
    // Donde dejar también la imagen de cada hoja que sale (HU-85: se guarda en la toma).
    // Llamar antes de start().
    void setImages(SheetImages* images) { m_images = images; }
    // Guardado automático (HU-86): cuando el lápiz lleva delayMs sin dibujar ni hay pedidos de
    // la pila, copia las hojas que cambiaron (el depósito y, la activa, su imagen) y se las
    // pasa a saver, que las escribe en su hilo; y toma.json si cambió el orden, la activa o la
    // vista. Al detenerse, lo que quedó sin pasar. La copia se corta si el lápiz toca el papel
    // (o llega un pedido) y se reintenta en la pausa siguiente: el trazo no la espera. Llamar
    // antes de start().
    static constexpr double kAutosaveDelayMs = 300;
    void setAutosave(TakeSaver* saver, double delayMs = kAutosaveDelayMs)
    {
        m_saver = saver;
        m_autosaveDelay = std::chrono::microseconds(int64_t(delayMs * 1000));
    }
    // Lo más que tardó una copia para guardar, en el hilo de simulación (diagnóstico).
    double autosaveCopyMaxMs() const { return m_autosaveCopyMaxMs; }
    // Cuántas hojas cambiaron y todavía no se pasaron al guardado (más el trazo en curso).
    int unsavedSheets() const { return m_unsaved; }
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
    // Lápiz de color (HU-99, Mesa de animación): 0..3 en vez de la mina (que sigue siendo la
    // del grafito al volver con -1). Como cambiar de mina: cierra el trazo en curso.
    void setColorPencil(int color) { m_colorPencil = color; }
    int colorPencil() const { return m_colorPencil; }
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

    // Roles de las hojas (HU-104), para toma.json: se guardan con la vista, en la pausa siguiente.
    void setRoles(SheetRoles roles)
    {
        {
            std::lock_guard lock(m_rotationMutex);
            m_roles = std::move(roles);
        }
        wake();
    }

    // La hoja fija (HU-89), por id, para toma.json (como los roles).
    void setFixedSheet(uint64_t id)
    {
        {
            std::lock_guard lock(m_rotationMutex);
            m_fixedSheet = id;
        }
        wake();
    }

    // La pila en círculo (HU-94), para toma.json.
    void setCircle(bool circle)
    {
        {
            std::lock_guard lock(m_rotationMutex);
            m_circle = circle;
        }
        wake();
    }

    // El audio de la toma (HU-106) y sus marcas (HU-108), para toma.json.
    void setAudio(QString file, std::vector<double> marks)
    {
        {
            std::lock_guard lock(m_rotationMutex);
            m_audio = std::move(file);
            m_audioMarks = std::move(marks);
        }
        wake();
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
    struct SheetCommand {
        enum Kind { Add, Insert, Duplicate, Clear, Remove, Move, Activate } kind;
        int a = 0, b = 0;
        uint64_t sample = 0; // cuántas muestras habían entrado a la cola (HU-90)
    };
    void pushSheetCommand(SheetCommand command);
    bool applySheetCommand(const SheetCommand& command); // true si cambió la hoja del papel
    void sheetChanged();      // repinta la hoja entera y lo anota en el log
    void publishSheetState(); // para la interfaz
    void renderAllParallel();  // toda la imagen, en varios hilos (hoja nueva activa)
    void retireSheet(uint64_t id); // deja la imagen de la activa para el render, antes de cambiarla
    // La imagen de la hoja activa, sin lo de afuera; vacía si keepGoing lo cortó.
    std::vector<uint32_t> activeRegion(const std::function<bool()>& keepGoing = {});
    void autosave(bool interruptible); // HU-86: copia lo que cambió y se lo pasa a m_saver
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
    SheetStack& m_sheets;
    std::mutex m_sheetMutex;
    std::vector<SheetCommand> m_sheetCommands;
    mutable std::mutex m_stateMutex;
    SheetState m_state;
    uint64_t m_applied = 0; // del hilo de simulación
    uint64_t m_notified = 0; // m_applied en el último aviso
    std::function<void()> m_onSheetState;
    uint64_t m_tag = 0;     // número de orden del deshacer (HU-87)
    std::atomic<double> m_activationMs{0};
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
    MediumProfile m_profile = MediumProfile::Illustration;
    uint32_t m_paperColor = kPaperColor;
    SheetImages* m_images = nullptr;
    TakeSaver* m_saver = nullptr;
    std::chrono::microseconds m_autosaveDelay{0};
    std::chrono::steady_clock::time_point m_lastActivity = std::chrono::steady_clock::now();
    std::optional<TakeIndex> m_savedIndex; // el último toma.json pasado a m_saver
    std::atomic<double> m_autosaveCopyMaxMs{0};
    std::atomic<int> m_unsaved{0};
    std::vector<uint32_t> m_guides; // la que usa el hilo de simulación
    std::mutex m_guidesMutex;
    std::vector<uint32_t> m_pendingGuides; // la que llega de afuera
    std::atomic<bool> m_guidesChanged{false};
    std::mutex m_rotationMutex;
    ViewRotation m_rotation;
    SheetRoles m_roles; // con m_rotationMutex
    uint64_t m_fixedSheet = 0; // HU-89, con m_rotationMutex
    bool m_circle = false;     // HU-94, ídem
    QString m_audio;           // HU-106, ídem
    std::vector<double> m_audioMarks; // HU-108, ídem
    std::atomic<uint64_t> m_lead;
    std::atomic<uint64_t> m_eraser;
    std::atomic<bool> m_erasing{false};
    std::atomic<bool> m_tilt{true};
    std::atomic<bool> m_stylus{false};
    std::atomic<int> m_colorPencil{-1};
    std::atomic<int> m_grade{kHbIndex};
    std::atomic<bool> m_sharpenRequested{false};
    std::atomic<int> m_wearPercent{0};
    PaperTexture m_texture;
    std::atomic<int> m_texturePercent{0};
    LightSchedule m_light;
};

} // namespace lienzo
