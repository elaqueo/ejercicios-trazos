#pragma once

#include "lienzo/SheetImages.h"

#include <QString>

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace drymedia {
class Paper;
}

namespace lienzo {

// Lo que guarda toma.json de la pila (HU-86): el orden de las hojas, la activa y la vista.
struct TakeIndex {
    std::vector<uint64_t> ids;
    int active = 0;
    double degrees = 0;
    bool operator==(const TakeIndex&) const = default;
};

// El guardado automático de la toma (HU-86, Mesa de animación): un hilo propio que comprime y
// escribe las hojas que le pasa la simulación (ya copiadas: la simulación sigue enseguida) y
// toma.json, con las piezas de lienzo/Take.h. Si llega otra copia de una hoja que todavía no
// escribió, se queda con la última; lo mismo con toma.json, que se escribe después de las
// hojas pendientes (así no nombra un archivo que todavía no existe y no borra uno nuevo).
// Seguro entre hilos.
class TakeSaver {
public:
    // paper: solo se lee su semilla y su tamaño, que no cambian. images: de dónde sacar la
    // imagen de una hoja que no es la activa (la que dejó al salir).
    TakeSaver(QString dir, const drymedia::Paper& paper, SheetImages* images);
    ~TakeSaver(); // escribe lo pendiente
    TakeSaver(const TakeSaver&) = delete;
    TakeSaver& operator=(const TakeSaver&) = delete;

    const QString& dir() const { return m_dir; }
    // state: el depósito (Paper::serializeTiles). pixels: la imagen de pantalla de la hoja,
    // width × height; vacía, la de images.
    void saveSheet(uint64_t id, std::vector<uint8_t> state, int width, int height, std::vector<uint32_t> pixels);
    void saveIndex(TakeIndex index);
    // Espera a que esté todo escrito.
    void flush();
    // Si hay algo por escribir o escribiéndose (HU-114: el panel dice "guardando…").
    bool pending() const
    {
        std::lock_guard lock(m_mutex);
        return !idle();
    }

    struct Stats {
        int sheets = 0;  // archivos de hoja escritos
        int indexes = 0; // toma.json escritos
        QString error;   // el último error al escribir (vacío si no hubo)
    };
    Stats stats() const;

private:
    struct Pending {
        std::vector<uint8_t> state;
        int width = 0, height = 0;
        std::vector<uint32_t> pixels;
    };
    void run();
    bool idle() const { return m_sheets.empty() && !m_index && !m_busy; }

    QString m_dir;
    const drymedia::Paper& m_paper;
    SheetImages* m_images;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake, m_done;
    std::map<uint64_t, Pending> m_sheets;
    std::optional<TakeIndex> m_index;
    bool m_busy = false, m_quit = false;
    Stats m_stats;
    std::thread m_thread;
};

} // namespace lienzo
