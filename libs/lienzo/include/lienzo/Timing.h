#pragma once

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace lienzo {

// Medición de tiempos para diagnosticar la latencia: muestras en ms con mediana, p95 y
// máximo. Segura entre hilos (la escriben la simulación y el render).
class TimingStat {
public:
    void add(double ms)
    {
        std::lock_guard lock(m_mutex);
        m_values.push_back(ms);
    }

    std::string describe(const char* name, const char* unit = "ms") const
    {
        std::lock_guard lock(m_mutex);
        if (m_values.empty())
            return std::string(name) + ": sin datos";
        std::vector<double> v = m_values;
        std::sort(v.begin(), v.end());
        const auto at = [&](double p) { return v[std::min(v.size() - 1, size_t(p * double(v.size())))]; };
        char text[200];
        snprintf(text, sizeof(text), "%s: n=%zu mediana %.3f %s · p95 %.3f %s · p99 %.3f %s · máx %.3f %s", name, v.size(),
                 at(0.5), unit, at(0.95), unit, at(0.99), unit, v.back(), unit);
        return text;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<double> m_values;
};

// Cronómetro con QueryPerformanceCounter.
class Stopwatch {
public:
    Stopwatch() { QueryPerformanceCounter(&m_start); }
    double ms() const
    {
        LARGE_INTEGER now, f;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&f);
        return double(now.QuadPart - m_start.QuadPart) * 1000.0 / double(f.QuadPart);
    }

private:
    LARGE_INTEGER m_start;
};

// Los tiempos que se miden en una sesión (se escriben al log al salir).
struct SessionTimings {
    TimingStat simBatch;     // simulación: lote completo
    TimingStat simPencil;    // simulación: lápiz (contacto + depósito)
    TimingStat simLockHeld;  // simulación: tiempo con la imagen bloqueada (tono)
    TimingStat simUndo;      // simulación: deshacer/rehacer + repintado
    TimingStat renderLockBusy; // render: 1 si encontró la imagen ocupada y no la tomó, 0 si la tomó
    TimingStat renderLatchToPresent; // render: desde que toma la imagen hasta Present
    TimingStat renderUploadPixels;   // render: píxeles subidos a la GPU por frame (no ms)
};

} // namespace lienzo
