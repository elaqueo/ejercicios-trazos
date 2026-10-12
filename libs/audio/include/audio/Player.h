#pragma once

// audio: reproducción del audio de la toma (HU-107) por WASAPI, en un hilo propio (no toca los
// de simulación ni render: RNF-04). Sin Qt.

#include "audio/Audio.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace audio {

class Player {
public:
    Player() = default;
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    // Reproduce `clip` desde el cuadro `from` en la salida por defecto de Windows (Windows
    // convierte la frecuencia y los canales a los del dispositivo). false, con el motivo, si
    // no se pudo abrir la salida. Si ya sonaba, empieza de nuevo desde `from`.
    bool play(std::shared_ptr<const Clip> clip, size_t from, std::string& error);
    // Detiene; la posición queda donde se escuchaba.
    void pause();
    // Sonando (false después de pausar o de llegar al final).
    bool playing() const { return m_playing; }
    // El cuadro que se está escuchando (lo escrito menos lo que todavía espera en el
    // dispositivo); al llegar al final, el último.
    size_t position() const { return m_position; }

private:
    void run(std::shared_ptr<const Clip> clip, size_t from);

    std::thread m_thread;
    std::atomic<bool> m_stop{false}, m_playing{false};
    std::atomic<size_t> m_position{0};
    // El resultado de abrir la salida, que espera play().
    std::mutex m_mutex;
    std::condition_variable m_startedSignal;
    bool m_started = false, m_ok = false;
    std::string m_error;
};

} // namespace audio
