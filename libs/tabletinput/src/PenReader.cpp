#include "tabletinput/PenReader.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tabletinput {

namespace {

constexpr double kDegrees = 180.0 / std::numbers::pi;
constexpr double kRadians = std::numbers::pi / 180.0;
// Más allá de esto la tangente explota; el hardware llega a ±60° (Intuos4).
constexpr float kMaxTilt = 89.0f;
// Windows da la presión en 0..1024.
constexpr float kMaxPressure = 1024.0f;

} // namespace

void tiltToAzimuthAltitude(float tiltX, float tiltY, float& azimuth, float& altitude)
{
    const double tx = std::tan(std::clamp(tiltX, -kMaxTilt, kMaxTilt) * kRadians);
    const double ty = std::tan(std::clamp(tiltY, -kMaxTilt, kMaxTilt) * kRadians);
    const double horizontal = std::sqrt(tx * tx + ty * ty); // proyección del lápiz por unidad de altura
    altitude = float(std::atan2(1.0, horizontal) * kDegrees);
    if (horizontal == 0.0) {
        azimuth = 0.0f; // vertical: sin dirección
        return;
    }
    double a = std::atan2(ty, tx) * kDegrees;
    if (a < 0)
        a += 360.0;
    azimuth = float(a);
}

int64_t qpcToMicroseconds(int64_t count, int64_t frequency)
{
    return count / frequency * 1000000 + count % frequency * 1000000 / frequency;
}

PenSample normalize(const POINTER_PEN_INFO& pen, const DeviceRects& rects, int64_t qpcFrequency)
{
    const POINTER_INFO& p = pen.pointerInfo;
    PenSample s;

    const double pw = double(rects.pointer.right - rects.pointer.left);
    const double ph = double(rects.pointer.bottom - rects.pointer.top);
    if (pw > 0 && ph > 0) {
        s.x = rects.display.left + (p.ptHimetricLocation.x - rects.pointer.left) *
                                       double(rects.display.right - rects.display.left) / pw;
        s.y = rects.display.top + (p.ptHimetricLocation.y - rects.pointer.top) *
                                      double(rects.display.bottom - rects.display.top) / ph;
        const double fx = (p.ptHimetricLocation.x - rects.pointer.left) / pw;
        const double fy = (p.ptHimetricLocation.y - rects.pointer.top) / ph;
        s.atEdge = fx <= kEdgeFraction || fx >= 1 - kEdgeFraction || fy <= kEdgeFraction || fy >= 1 - kEdgeFraction;
    } else {
        s.x = p.ptPixelLocation.x; // sin rectángulos: lo mejor que hay, cuantizado
        s.y = p.ptPixelLocation.y;
    }

    s.inContact = (p.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
    if (pen.penMask & PEN_MASK_PRESSURE)
        s.pressure = std::min(1.0f, float(pen.pressure) / kMaxPressure);
    else
        s.pressure = s.inContact ? 1.0f : 0.0f;
    if (pen.penMask & PEN_MASK_TILT_X)
        s.tiltX = float(pen.tiltX);
    if (pen.penMask & PEN_MASK_TILT_Y)
        s.tiltY = float(pen.tiltY);
    tiltToAzimuthAltitude(s.tiltX, s.tiltY, s.azimuth, s.altitude);
    if (pen.penMask & PEN_MASK_ROTATION)
        s.rotation = float(pen.rotation);

    s.timeUs = qpcToMicroseconds(int64_t(p.PerformanceCount), qpcFrequency);
    s.eraser = (pen.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) != 0;
    s.barrel = (pen.penFlags & PEN_FLAG_BARREL) != 0;
    return s;
}

void appendHistory(const POINTER_PEN_INFO* history, uint32_t count, const DeviceRects& rects, int64_t qpcFrequency,
                   std::vector<PenSample>& out)
{
    for (uint32_t i = count; i-- > 0;) // la más nueva viene primero
        out.push_back(normalize(history[i], rects, qpcFrequency));
}

PenReader::PenReader()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    m_qpcFrequency = frequency.QuadPart;
}

bool PenReader::handleMessage(HWND, UINT message, WPARAM wParam, LPARAM, std::vector<PenSample>& out)
{
    if (message != WM_POINTERDOWN && message != WM_POINTERUPDATE && message != WM_POINTERUP)
        return false;
    const UINT32 id = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO info{};
    if (!GetPointerInfo(id, &info) || info.pointerType != PT_PEN)
        return false;

    if (info.sourceDevice != m_device) {
        m_device = info.sourceDevice;
        m_rects = {};
        GetPointerDeviceRects(m_device, &m_rects.pointer, &m_rects.display);
    }

    UINT32 count = std::max<UINT32>(info.historyCount, 1);
    m_history.resize(count);
    if (!GetPointerPenInfoHistory(id, &count, m_history.data()))
        return true; // es del lápiz igual: no dejar que Windows lo convierta en mouse
    appendHistory(m_history.data(), count, m_rects, m_qpcFrequency, out);
    return true;
}

} // namespace tabletinput
