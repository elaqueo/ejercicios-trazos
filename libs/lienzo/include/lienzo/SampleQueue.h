#pragma once

#include <tabletinput/PenReader.h>

#include <windows.h>

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
        }
        SetEvent(m_event);
    }

    // Deja en out todo lo encolado (y vacía la cola).
    void takeAll(std::vector<tabletinput::PenSample>& out)
    {
        out.clear();
        std::lock_guard lock(m_mutex);
        out.swap(m_samples);
    }

    // Espera hasta que haya muestras (o pase timeoutMs). También la despierta wake().
    void wait(DWORD timeoutMs) { WaitForSingleObject(m_event, timeoutMs); }
    void wake() { SetEvent(m_event); }

private:
    std::mutex m_mutex;
    std::vector<tabletinput::PenSample> m_samples;
    HANDLE m_event;
};

} // namespace lienzo
