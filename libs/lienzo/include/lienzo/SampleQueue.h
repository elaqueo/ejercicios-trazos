#pragma once

#include <tabletinput/PenReader.h>

#include <windows.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace lienzo {

// Cola de muestras del hilo de entrada al de simulación. Todas las muestras, en orden;
// el consumidor se lleva el lote entero de una vez. Un evento despierta al consumidor.
class SampleQueue {
public:
    SampleQueue()
        : m_event(CreateEventW(nullptr, FALSE, FALSE, nullptr))
    {
    }
    ~SampleQueue() { CloseHandle(m_event); }
    SampleQueue(const SampleQueue&) = delete;
    SampleQueue& operator=(const SampleQueue&) = delete;

    void push(const std::vector<tabletinput::PenSample>& samples)
    {
        if (samples.empty())
            return;
        {
            std::lock_guard lock(m_mutex);
            m_samples.insert(m_samples.end(), samples.begin(), samples.end());
            m_pushed += samples.size();
        }
        SetEvent(m_event);
    }

    // Cuántas muestras entraron desde el principio: el número de orden de la próxima (HU-90,
    // para ubicar los pedidos de la pila entre las muestras).
    uint64_t pushed() const
    {
        std::lock_guard lock(m_mutex);
        return m_pushed;
    }

    // Deja en out todo lo encolado (y vacía la cola). Devuelve el número de orden de out[0].
    uint64_t takeAll(std::vector<tabletinput::PenSample>& out)
    {
        out.clear();
        std::lock_guard lock(m_mutex);
        out.swap(m_samples);
        return m_pushed - out.size();
    }

    // Espera hasta que haya muestras (o pase timeoutMs). También la despierta wake().
    void wait(DWORD timeoutMs) { WaitForSingleObject(m_event, timeoutMs); }
    void wake() { SetEvent(m_event); }

private:
    mutable std::mutex m_mutex;
    std::vector<tabletinput::PenSample> m_samples;
    uint64_t m_pushed = 0;
    HANDLE m_event;
};

} // namespace lienzo
