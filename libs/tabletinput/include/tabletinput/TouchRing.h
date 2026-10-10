#pragma once

// tabletinput: la rueda táctil de la tableta (Touch Ring de la Intuos4; HU-74), por las
// extensiones de Wintab. El lápiz sigue entrando por Windows Ink (PenReader); de Wintab se
// usa solo la rueda: la app le pide al driver que, mientras tiene el foco, la rueda le
// mande la posición del dedo en vez de hacer lo configurado en el panel de Wacom.
// wintab32.dll se carga en tiempo de ejecución: sin driver de Wacom, open() devuelve false.

#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tabletinput {

struct RingEvent {
    int tablet = 0, control = 0, mode = 0;
    uint32_t position = 0; // posición cruda del dedo (ver minimum/maximum)
};

// Posiciones de la rueda → grados de giro (pura, se prueba sin tableta). Una vuelta del dedo
// es una vuelta de la hoja: con 72 posiciones, 5° por posición. En el sentido de las agujas
// del reloj las posiciones suben (…71, 72, 1, 2…) y la hoja gira igual (grados positivos);
// 0 = sin dedo.
class RingTracker {
public:
    static constexpr int64_t kBounceUs = 150000; // un 0 más corto que esto no corta el giro…
    static constexpr int kBounceSteps = 4;       // …si el dedo vuelve cerca de donde estaba

    struct Step {
        double degrees = 0; // cuánto girar (0 si no se movió)
        bool ended = false; // se levantó el dedo: guardar el giro
    };

    explicit RingTracker(int positions = 72) : m_positions(positions) {}
    Step feed(uint32_t position, int64_t timeUs);

private:
    int wrap(int delta) const; // la diferencia más corta, cruzando de 72 a 1

    int m_positions;
    int m_last = 0;
    bool m_hasLast = false, m_touching = false;
    int64_t m_liftUs = 0;
};

class TouchRing {
public:
    using Log = std::function<void(const std::string&)>;

    TouchRing() = default;
    ~TouchRing();
    TouchRing(const TouchRing&) = delete;
    TouchRing& operator=(const TouchRing&) = delete;

    // Abre un contexto de Wintab en hwnd (los mensajes llegan a su WndProc) y toma la rueda.
    // log recibe lo que pasa (diagnóstico). name (UTF-8) es lo que muestra el driver para la
    // rueda al apretar el botón central. false si no hay Wintab o la tableta no tiene rueda.
    bool open(HWND hwnd, Log log, std::string name = {});
    void close();
    bool isOpen() const { return m_context != nullptr; }

    // Para llamar desde el WndProc: si es un paquete de la rueda, agrega el evento y
    // devuelve true.
    bool handleMessage(UINT message, WPARAM wParam, LPARAM lParam, std::vector<RingEvent>& out);

    // Rango de posiciones que informa el driver (Intuos4: 72 por vuelta).
    uint32_t minimum() const { return m_min; }
    uint32_t maximum() const { return m_max; }

private:
    struct Api;
    void overrideControls(bool enable);

    Api* m_api = nullptr;
    void* m_context = nullptr;
    Log m_log;
    std::string m_name;
    uint32_t m_min = 0, m_max = 0;
    int m_controls = 0;
    std::vector<int> m_functions; // funciones (modos) por control
    size_t m_ringOffset = 0;      // dónde viene la rueda dentro del paquete
    uint32_t m_extMask = 0;
};

} // namespace tabletinput
