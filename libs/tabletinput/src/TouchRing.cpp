#include "tabletinput/TouchRing.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace tabletinput {

namespace {

// Lo mínimo de wintab.h / pktdef.h (Wacom publica los encabezados; se copian solo las
// constantes y estructuras que se usan).
using HCTX = HANDLE;
using WTPKT = DWORD;
using FIX32 = DWORD;

constexpr UINT WTI_DEFCONTEXT = 3;
constexpr UINT WTI_EXTENSIONS = 300;
constexpr UINT EXT_TAG = 2, EXT_MASK = 3;
constexpr UINT WTX_CSRMASK = 3, WTX_TOUCHSTRIP = 6, WTX_TOUCHRING = 7, WTX_EXPKEYS2 = 8;
constexpr UINT CXO_MESSAGES = 0x0004;
constexpr UINT WT_DEFBASE = 0x7FF0;
constexpr UINT WT_PACKETEXT = WT_DEFBASE + 8;
constexpr WORD TABLET_PROPERTY_CONTROLCOUNT = 0, TABLET_PROPERTY_FUNCCOUNT = 1, TABLET_PROPERTY_MIN = 3,
               TABLET_PROPERTY_MAX = 4, TABLET_PROPERTY_OVERRIDE = 5;

struct LOGCONTEXTW {
    WCHAR lcName[40];
    UINT lcOptions, lcStatus, lcLocks, lcMsgBase, lcDevice, lcPktRate;
    WTPKT lcPktData, lcPktMode, lcMoveMask;
    DWORD lcBtnDnMask, lcBtnUpMask;
    LONG lcInOrgX, lcInOrgY, lcInOrgZ, lcInExtX, lcInExtY, lcInExtZ;
    LONG lcOutOrgX, lcOutOrgY, lcOutOrgZ, lcOutExtX, lcOutExtY, lcOutExtZ;
    FIX32 lcSensX, lcSensY, lcSensZ;
    BOOL lcSysMode;
    int lcSysOrgX, lcSysOrgY, lcSysExtX, lcSysExtY;
    FIX32 lcSysSensX, lcSysSensY;
};

struct EXTPROPERTY {
    BYTE version, tabletIndex, controlIndex, functionIndex;
    WORD propertyID, reserved;
    DWORD dataSize;
    BYTE data[16];
};

// Paquete de extensiones: la base y después cada extensión pedida, en este orden.
struct EXTENSIONBASE {
    HCTX nContext;
    UINT nStatus;
    DWORD nTime;
    UINT nSerialNumber;
};
struct EXPKEYSDATA {
    BYTE nTablet, nControl, nLocation, nReserved;
    DWORD nState;
};
struct SLIDERDATA {
    BYTE nTablet, nControl, nMode, nReserved;
    DWORD nPosition;
};
struct PACKETEXT {
    EXTENSIONBASE pkBase;
    EXPKEYSDATA pkExpKeys;
    SLIDERDATA pkTouchStrip;
    SLIDERDATA pkTouchRing;
};

std::string format(const char* fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof buffer, fmt, args);
    va_end(args);
    return buffer;
}

} // namespace

struct TouchRing::Api {
    HMODULE dll = nullptr;
    UINT(WINAPI* info)(UINT, UINT, LPVOID) = nullptr;
    HCTX(WINAPI* open)(HWND, LOGCONTEXTW*, BOOL) = nullptr;
    BOOL(WINAPI* close)(HCTX) = nullptr;
    BOOL(WINAPI* packet)(HCTX, UINT, LPVOID) = nullptr;
    BOOL(WINAPI* extGet)(HCTX, UINT, LPVOID) = nullptr;
    BOOL(WINAPI* extSet)(HCTX, UINT, LPVOID) = nullptr;
    BOOL(WINAPI* overlap)(HCTX, BOOL) = nullptr;

    bool load()
    {
        dll = LoadLibraryW(L"wintab32.dll");
        if (!dll)
            return false;
        const auto get = [this](auto& fn, const char* name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(GetProcAddress(dll, name));
            return fn != nullptr;
        };
        return get(info, "WTInfoW") && get(open, "WTOpenW") && get(close, "WTClose") && get(packet, "WTPacket") &&
               get(extGet, "WTExtGet") && get(extSet, "WTExtSet") && get(overlap, "WTOverlap");
    }
    ~Api()
    {
        if (dll)
            FreeLibrary(dll);
    }
};

int RingTracker::wrap(int delta) const
{
    const int half = m_positions / 2;
    return ((delta % m_positions) + m_positions + half) % m_positions - half;
}

RingTracker::Step RingTracker::feed(uint32_t position, int64_t timeUs)
{
    if (position == 0 || int(position) > m_positions) {
        if (!m_touching)
            return {};
        m_touching = false;
        m_liftUs = timeUs;
        return {0, true};
    }
    const int p = int(position);
    if (!m_touching) {
        m_touching = true;
        // Un 0 suelto en medio de una vuelta (el dedo apenas se despega) no corta el giro.
        const bool resume = m_hasLast && timeUs - m_liftUs <= kBounceUs && std::abs(wrap(p - m_last)) <= kBounceSteps;
        if (!resume) {
            m_last = p;
            m_hasLast = true;
            return {}; // apoyar el dedo no gira: gira lo que se desliza
        }
    }
    const int steps = wrap(p - m_last);
    m_last = p;
    return {steps * 360.0 / m_positions, false};
}

TouchRing::~TouchRing()
{
    close();
}

bool TouchRing::open(HWND hwnd, Log log)
{
    m_log = std::move(log);
    const auto say = [this](const std::string& s) {
        if (m_log)
            m_log(s);
    };
    m_api = new Api;
    if (!m_api->load()) {
        say("Rueda: no hay Wintab (wintab32.dll); sin rueda");
        close();
        return false;
    }

    // Qué extensiones ofrece el driver y con qué máscara se piden.
    uint32_t ringMask = 0, stripMask = 0, keysMask = 0;
    for (UINT i = 0;; ++i) {
        UINT tag = 0;
        if (!m_api->info(WTI_EXTENSIONS + i, EXT_TAG, &tag))
            break;
        WTPKT mask = 0;
        m_api->info(WTI_EXTENSIONS + i, EXT_MASK, &mask);
        if (tag == WTX_TOUCHRING)
            ringMask = mask;
        else if (tag == WTX_TOUCHSTRIP)
            stripMask = mask;
        else if (tag == WTX_EXPKEYS2)
            keysMask = mask;
    }
    if (!ringMask) {
        say("Rueda: el driver no ofrece la extensión de rueda");
        close();
        return false;
    }
    // El paquete trae las extensiones pedidas en orden: se piden las tres que haya y se
    // calcula dónde cae la rueda.
    m_extMask = ringMask | stripMask | keysMask;
    m_ringOffset = sizeof(EXTENSIONBASE) + (keysMask ? sizeof(EXPKEYSDATA) : 0) + (stripMask ? sizeof(SLIDERDATA) : 0);

    LOGCONTEXTW lc{};
    if (!m_api->info(WTI_DEFCONTEXT, 0, &lc)) {
        say("Rueda: Wintab no da el contexto por defecto");
        close();
        return false;
    }
    wcscpy_s(lc.lcName, L"Trazos rueda");
    lc.lcOptions = CXO_MESSAGES; // sin CXO_SYSTEM: no mueve el cursor
    lc.lcMsgBase = WT_DEFBASE;
    lc.lcPktData = m_extMask;
    lc.lcPktMode = 0;
    lc.lcMoveMask = 0;
    lc.lcBtnDnMask = lc.lcBtnUpMask = 0;
    m_context = m_api->open(hwnd, &lc, TRUE);
    if (!m_context) {
        say("Rueda: WTOpen falló");
        close();
        return false;
    }
    m_api->overlap(m_context, TRUE);
    // Que el contexto no reciba el lápiz (ningún cursor): si no, el driver le manda el lápiz
    // a Wintab y deja de llegar por Windows Ink (el lápiz se clavaba a los pocos milímetros).
    BYTE noCursors[16]{};
    if (!m_api->extSet(m_context, WTX_CSRMASK, noCursors)) {
        say("Rueda: no se pudo dejar el lápiz fuera de Wintab; sin rueda");
        close();
        return false;
    }

    // Controles (ruedas) de la tableta 0, sus funciones (modos del botón central) y el rango.
    EXTPROPERTY p{};
    p.dataSize = sizeof(UINT32);
    p.propertyID = TABLET_PROPERTY_CONTROLCOUNT;
    if (m_api->extGet(m_context, WTX_TOUCHRING, &p))
        std::memcpy(&m_controls, p.data, sizeof(UINT32));
    m_functions.assign(size_t(m_controls), 0);
    for (int c = 0; c < m_controls; ++c) {
        EXTPROPERTY q{};
        q.controlIndex = BYTE(c);
        q.dataSize = sizeof(UINT32);
        q.propertyID = TABLET_PROPERTY_FUNCCOUNT;
        if (m_api->extGet(m_context, WTX_TOUCHRING, &q))
            std::memcpy(&m_functions[size_t(c)], q.data, sizeof(UINT32));
        q.propertyID = TABLET_PROPERTY_MIN;
        if (m_api->extGet(m_context, WTX_TOUCHRING, &q))
            std::memcpy(&m_min, q.data, sizeof(UINT32));
        q.propertyID = TABLET_PROPERTY_MAX;
        if (m_api->extGet(m_context, WTX_TOUCHRING, &q))
            std::memcpy(&m_max, q.data, sizeof(UINT32));
    }
    if (m_controls == 0) {
        say("Rueda: la tableta no tiene rueda");
        close();
        return false;
    }
    overrideControls(true);
    say(format("Rueda: lista (%d control, %d modos, posiciones %u a %u)", m_controls, m_functions.empty() ? 0 : m_functions[0],
               m_min, m_max));
    return true;
}

void TouchRing::overrideControls(bool enable)
{
    if (!m_api || !m_context)
        return;
    for (int c = 0; c < m_controls; ++c)
        for (int f = 0; f < m_functions[size_t(c)]; ++f) {
            EXTPROPERTY p{};
            p.controlIndex = BYTE(c);
            p.functionIndex = BYTE(f);
            p.propertyID = TABLET_PROPERTY_OVERRIDE;
            p.dataSize = sizeof(BOOL);
            const BOOL value = enable ? TRUE : FALSE;
            std::memcpy(p.data, &value, sizeof value);
            if (!m_api->extSet(m_context, WTX_TOUCHRING, &p) && m_log)
                m_log(format("Rueda: no se pudo %s el modo %d", enable ? "tomar" : "devolver", f));
        }
}

void TouchRing::close()
{
    if (m_api && m_context) {
        overrideControls(false);
        m_api->close(m_context);
    }
    m_context = nullptr;
    delete m_api;
    m_api = nullptr;
}

bool TouchRing::handleMessage(UINT message, WPARAM wParam, LPARAM lParam, std::vector<RingEvent>& out)
{
    if (message != WT_PACKETEXT || !m_context)
        return false;
    // El paquete trae su contexto en lParam, que no siempre es el valor que devolvió WTOpen
    // (medido en la Intuos4: abre 0x802, los paquetes llegan con 0x902): se usa ese.
    alignas(8) BYTE buffer[sizeof(PACKETEXT) + 32]{};
    if (!m_api->packet(reinterpret_cast<HCTX>(lParam), UINT(wParam), buffer))
        return true;
    SLIDERDATA ring;
    std::memcpy(&ring, buffer + m_ringOffset, sizeof ring);
    out.push_back({ring.nTablet, ring.nControl, ring.nMode, uint32_t(ring.nPosition)});
    return true;
}

} // namespace tabletinput
