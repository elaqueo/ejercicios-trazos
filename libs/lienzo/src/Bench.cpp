#include "lienzo/Bench.h"

#include "lienzo/SampleQueue.h"
#include "lienzo/Simulation.h"

#include <tabletinput/PenReader.h>

#include <QCoreApplication>
#include <QMetaObject>

#include <cmath>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace lienzo {

namespace {

constexpr double kSampleRate = 133.0;
constexpr double kDurationS = 30.0;
constexpr int kStrokeSamples = 160; // ~1,2 s por trazo
constexpr int kPauseSamples = 40;   // ~0,3 s sin apoyar
constexpr int kStrokesPerUndo = 4;

int64_t nowUs()
{
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return tabletinput::qpcToMicroseconds(c.QuadPart, f.QuadPart);
}

} // namespace

void runBench(SampleQueue& queue, Simulation& sim, const SheetMapping& m, bool withUndo)
{
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    const int64_t periodUs = int64_t(1e6 / kSampleRate);
    const int total = int(kDurationS * kSampleRate);
    const double cx = m.sheetX + m.sheetWidth / 2.0, cy = m.sheetY + m.sheetHeight / 2.0;
    const double rx = m.sheetWidth * 0.4, ry = m.sheetHeight * 0.4;
    int64_t next = nowUs();
    int stroke = 0;

    for (int i = 0; i < total; ++i) {
        const int inCycle = i % (kStrokeSamples + kPauseSamples);
        tabletinput::PenSample s;
        const double t = i * 0.012;
        s.x = cx + rx * std::sin(t * 1.3 + stroke);
        s.y = cy + ry * std::sin(t * 2.1 + stroke * 0.7);
        s.pressure = float(0.5 + 0.2 * std::sin(t * 5));
        s.altitude = 70;
        s.inContact = inCycle < kStrokeSamples;
        s.timeUs = nowUs();
        queue.push({s});

        if (inCycle == kStrokeSamples) { // terminó un trazo
            ++stroke;
            if (withUndo && stroke % kStrokesPerUndo == 0) {
                for (int k = 0; k < 3; ++k)
                    sim.requestUndo();
                for (int k = 0; k < 3; ++k)
                    sim.requestRedo();
            }
        }

        next += periodUs;
        const int64_t wait = next - nowUs();
        if (wait > 0 && timer) {
            LARGE_INTEGER due;
            due.QuadPart = -wait * 10; // unidades de 100 ns
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, 100);
        }
    }
    if (timer)
        CloseHandle(timer);
    QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
}

} // namespace lienzo
