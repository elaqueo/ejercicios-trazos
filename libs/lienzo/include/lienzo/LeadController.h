#pragma once

#include <algorithm>

namespace lienzo {

// Adelanto del render justo a tiempo (spike HU-44): cuánto antes del vsync se toman las
// muestras y se dibuja. Poco adelanto baja la latencia, pero si el frame llega tarde a la
// composición se pierde un vsync entero. Arranca en el valor seguro medido (7,5 ms); un
// vsync perdido lo sube de golpe, y una racha larga sin pérdidas lo baja de a poco.
class LeadController {
public:
    static constexpr double kStartMs = 7.5;
    static constexpr double kMinMs = 5.0;  // por debajo de ~4 ms se perdían frames (HU-44)
    static constexpr double kMaxMs = 12.0;
    static constexpr double kMissStepMs = 1.0;
    static constexpr double kCalmStepMs = 0.25;
    static constexpr int kCalmFrames = 300; // 5 s a 60 Hz

    void onFrame(bool missedVsync)
    {
        if (missedVsync) {
            m_leadMs = std::min(kMaxMs, m_leadMs + kMissStepMs);
            m_calm = 0;
            ++m_missed;
        } else if (++m_calm >= kCalmFrames) {
            m_leadMs = std::max(kMinMs, m_leadMs - kCalmStepMs);
            m_calm = 0;
        }
    }

    double leadMs() const { return m_leadMs; }
    int missed() const { return m_missed; }

private:
    double m_leadMs = kStartMs;
    int m_calm = 0;
    int m_missed = 0;
};

} // namespace lienzo
