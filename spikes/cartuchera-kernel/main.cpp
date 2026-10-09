// Spike HU-45 (Cartuchera, Fase 0): ¿entra el kernel de contacto y depósito en el
// presupuesto de CPU? Mide el tiempo por muestra de lápiz con trazos sintéticos
// (semilla fija, 133 muestras/s como en HU-43) en las rutas escalar, AVX2 y AVX-512, y
// verifica que las tres dejan el papel idéntico (hash).
//
// Uso: cartuchera-kernel.exe [--sin-pausa]
// Escribe el resultado en la consola y en kernel-<equipo>-<fecha>.txt junto al exe.

#include "kernel.h"

#include <windows.h>

#include <intrin.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kTile = 64;
constexpr double kCellsPerMm = 600.0 / 25.4; // 600 dpi
constexpr double kSampleRate = 133.0;        // medido en HU-43
constexpr int kSamples = 1500;               // ~11 s de trazo por caso
constexpr int kSearchSteps = 16;             // búsqueda binaria sobre D (u16)

LARGE_INTEGER g_freq{};
FILE* g_out = nullptr;

void print(const char* format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    fputs(line, stdout);
    if (g_out)
        fputs(line, g_out);
}

// --- Capacidades de la CPU -----------------------------------------------------------

bool hasAvx2()
{
    int r[4];
    __cpuid(r, 1);
    const bool osxsave = (r[2] & (1 << 27)) != 0, avx = (r[2] & (1 << 28)) != 0;
    if (!osxsave || !avx || (_xgetbv(0) & 0x6) != 0x6)
        return false;
    __cpuidex(r, 7, 0);
    return (r[1] & (1 << 5)) != 0;
}

bool hasAvx512()
{
    if (!hasAvx2())
        return false;
    if ((_xgetbv(0) & 0xE6) != 0xE6) // el sistema guarda los registros de AVX-512
        return false;
    int r[4];
    __cpuidex(r, 7, 0);
    const bool f = (r[1] & (1 << 16)) != 0, bw = (r[1] & (1 << 30)) != 0;
    return f && bw;
}

std::string cpuName()
{
    int r[4];
    char name[49] = {};
    for (int i = 0; i < 3; ++i) {
        __cpuid(r, 0x80000002 + i);
        memcpy(name + i * 16, r, 16);
    }
    std::string s(name);
    s.erase(0, s.find_first_not_of(' '));
    return s;
}

// --- Papel en tiles dispersos ----------------------------------------------------------

uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

struct Tile {
    uint16_t relief[kTile * kTile];
    uint16_t deposit[kTile * kTile];
};

class Paper {
public:
    Tile& tile(int tx, int ty)
    {
        const int64_t key = (int64_t(ty) << 32) | uint32_t(tx);
        auto it = m_tiles.find(key);
        if (it != m_tiles.end())
            return *it->second;
        auto t = std::make_unique<Tile>();
        // Relieve procedural determinista: grano fino + ondulación más ancha.
        for (int y = 0; y < kTile; ++y)
            for (int x = 0; x < kTile; ++x) {
                const uint32_t gx = uint32_t(tx * kTile + x), gy = uint32_t(ty * kTile + y);
                const uint32_t grano = hash32(gx * 73856093u ^ gy * 19349663u) >> 21;          // 0..2047
                const uint32_t ancho = hash32((gx / 6) * 83492791u ^ (gy / 6) * 2654435761u) >> 21; // 0..2047
                t->relief[y * kTile + x] = uint16_t(grano + ancho);
            }
        memset(t->deposit, 0, sizeof(t->deposit));
        Tile& ref = *t;
        m_tiles.emplace(key, std::move(t));
        return ref;
    }

    // Copia (o devuelve) un rectángulo w × h que arranca en la celda (x0, y0).
    template <bool kWrite>
    void transfer(int x0, int y0, int w, int h, uint16_t* relief, uint16_t* deposit)
    {
        for (int r = 0; r < h; ++r) {
            const int gy = y0 + r;
            int x = 0;
            while (x < w) {
                const int gx = x0 + x;
                const int tx = gx / kTile, ty = gy / kTile; // coordenadas siempre positivas
                const int lx = gx % kTile, ly = gy % kTile;
                const int run = std::min(w - x, kTile - lx);
                Tile& t = tile(tx, ty);
                if (kWrite) {
                    memcpy(&t.deposit[ly * kTile + lx], deposit + r * w + x, size_t(run) * 2);
                } else {
                    memcpy(relief + r * w + x, &t.relief[ly * kTile + lx], size_t(run) * 2);
                    memcpy(deposit + r * w + x, &t.deposit[ly * kTile + lx], size_t(run) * 2);
                }
                x += run;
            }
        }
    }

    uint64_t hash() const
    {
        uint64_t h = 1469598103934665603ULL; // FNV-1a, en orden de clave (std::map)
        for (const auto& [key, t] : m_tiles) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(&key);
            for (size_t i = 0; i < sizeof(key); ++i)
                h = (h ^ bytes[i]) * 1099511628211ULL;
            bytes = reinterpret_cast<const uint8_t*>(t->deposit);
            for (size_t i = 0; i < sizeof(t->deposit); ++i)
                h = (h ^ bytes[i]) * 1099511628211ULL;
        }
        return h;
    }

    size_t tileCount() const { return m_tiles.size(); }

private:
    std::map<int64_t, std::unique_ptr<Tile>> m_tiles;
};

// --- Punta y casos ---------------------------------------------------------------------

struct Case {
    const char* name;
    int w, h;        // huella en celdas (múltiplo de 32 en total)
    double speedMm;  // mm/s
};

const Case kCases[] = {
    {"punta HB, trazo normal", 24, 24, 100},
    {"punta HB, garabato rápido", 24, 24, 1000},
    {"lápiz de costado", 64, 64, 300},
    {"barra de pastel de costado", 128, 32, 300},
};

// Punta: elipse con forma de cono; 0 en el centro, crece hacia el borde.
std::vector<uint16_t> makeTip(int w, int h)
{
    std::vector<uint16_t> tip(size_t(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double dx = (x + 0.5 - w / 2.0) / (w / 2.0), dy = (y + 0.5 - h / 2.0) / (h / 2.0);
            const double r = std::sqrt(dx * dx + dy * dy);
            tip[size_t(y) * w + x] = r >= 1.0 ? 65535 : uint16_t(r * 6000.0);
        }
    return tip;
}

struct Sample {
    int64_t x, y;      // celdas × 256
    uint32_t pressure; // 0..256
};

// Trazo sintético: una curva tipo Lissajous recorrida a velocidad constante, con
// presión ondulante. Mismo trazo para las tres rutas.
std::vector<Sample> makeStroke(double speedMm)
{
    std::vector<Sample> s;
    s.reserve(kSamples);
    const double cx = 3500, cy = 2500, rx = 2400, ry = 1600; // dentro de una A4 a 600 dpi
    double t = 0;
    for (int i = 0; i < kSamples; ++i) {
        const double x = cx + rx * std::sin(t), y = cy + ry * std::sin(2.3 * t + 0.7);
        const double vx = rx * std::cos(t), vy = 2.3 * ry * std::cos(2.3 * t + 0.7);
        const double v = std::max(1.0, std::sqrt(vx * vx + vy * vy)); // celdas por unidad de t
        t += speedMm * kCellsPerMm / kSampleRate / v;
        const double p = 0.6 + 0.3 * std::sin(i * 0.05);
        s.push_back({int64_t(std::llround(x * 256)), int64_t(std::llround(y * 256)), uint32_t(p * 256)});
    }
    return s;
}

struct Result {
    std::vector<double> times; // ms por muestra
    uint64_t hash = 0;
    size_t tiles = 0;
    uint64_t substeps = 0;
};

Result run(const kernel::Impl& impl, const Case& c, const std::vector<Sample>& stroke)
{
    Paper paper;
    const int n = c.w * c.h;
    const std::vector<uint16_t> tip = makeTip(c.w, c.h);
    const size_t cells = static_cast<size_t>(n);
    std::vector<uint16_t> relief(cells), deposit(cells), surface(cells);
    Result result;
    result.times.reserve(stroke.size());

    for (size_t i = 1; i < stroke.size(); ++i) {
        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);

        const Sample& a = stroke[i - 1];
        const Sample& b = stroke[i];
        // Subpasos de como mucho una celda (distancia de Chebyshev).
        const int64_t dx = b.x - a.x, dy = b.y - a.y;
        const int64_t len = std::max(std::llabs(dx), std::llabs(dy));
        const int steps = int(std::max<int64_t>(1, (len + 255) / 256));
        // Depósito ∝ distancia del subpaso (celdas × 256, ≤ 256) × blandura.
        const uint16_t k = uint16_t(std::min<int64_t>(65535, (len / steps) * 16));

        for (int s = 1; s <= steps; ++s) {
            const int64_t x = a.x + dx * s / steps, y = a.y + dy * s / steps;
            const int64_t p = int64_t(a.pressure) + (int64_t(b.pressure) - int64_t(a.pressure)) * s / steps;
            const uint32_t target = uint32_t(p) * uint32_t(n) * 2; // fuerza buscada
            const int x0 = int(x >> 8) - c.w / 2, y0 = int(y >> 8) - c.h / 2;

            paper.transfer<false>(x0, y0, c.w, c.h, relief.data(), deposit.data());
            impl.surface(surface.data(), relief.data(), deposit.data(), n);

            // Mayor D (punta más alta) con fuerza ≥ objetivo: la profundidad de contacto.
            uint32_t lo = 0, hi = 65535;
            for (int it = 0; it < kSearchSteps; ++it) {
                const uint32_t mid = (lo + hi + 1) / 2;
                if (impl.force(surface.data(), tip.data(), n, uint16_t(mid)) >= target)
                    lo = mid;
                else
                    hi = mid - 1;
            }
            impl.deposit(deposit.data(), surface.data(), tip.data(), n, uint16_t(lo), k);
            paper.transfer<true>(x0, y0, c.w, c.h, nullptr, deposit.data());
        }
        result.substeps += uint64_t(steps);

        QueryPerformanceCounter(&t1);
        result.times.push_back(double(t1.QuadPart - t0.QuadPart) * 1000.0 / double(g_freq.QuadPart));
    }
    result.hash = paper.hash();
    result.tiles = paper.tileCount();
    return result;
}

double percentile(std::vector<double> v, double p)
{
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(p * double(v.size())))];
}

} // namespace

int main(int argc, char* argv[])
{
    const bool pause = !(argc > 1 && strcmp(argv[1], "--sin-pausa") == 0);
    QueryPerformanceFrequency(&g_freq);
    SetConsoleOutputCP(CP_UTF8);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    char computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD size = sizeof(computer);
    GetComputerNameA(computer, &size);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    dir.resize(dir.find_last_of('\\') + 1);
    char file[MAX_PATH];
    snprintf(file, sizeof(file), "%skernel-%s-%04u%02u%02u-%02u%02u.txt", dir.c_str(), computer, st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute);
    fopen_s(&g_out, file, "w");

    const bool avx2 = hasAvx2(), avx512 = hasAvx512();
    print("Spike HU-45 · kernel de contacto de Cartuchera\n");
    print("equipo %s · %s · AVX2 %s · AVX-512 %s\n", computer, cpuName().c_str(), avx2 ? "sí" : "no",
          avx512 ? "sí" : "no");
    print("%d muestras por caso a %.0f/s, 600 dpi, búsqueda de %d pasos\n\n", kSamples, kSampleRate, kSearchSteps);

    std::vector<const kernel::Impl*> impls{&kernel::kScalar};
    if (avx2)
        impls.push_back(&kernel::kAvx2);
    if (avx512)
        impls.push_back(&kernel::kAvx512);

    bool allEqual = true;
    for (const Case& c : kCases) {
        const std::vector<Sample> stroke = makeStroke(c.speedMm);
        print("== %s (huella %dx%d = %d celdas, %.0f mm/s)\n", c.name, c.w, c.h, c.w * c.h, c.speedMm);
        uint64_t reference = 0;
        for (const kernel::Impl* impl : impls) {
            const Result r = run(*impl, c, stroke);
            if (impl == impls.front())
                reference = r.hash;
            const bool equal = r.hash == reference;
            allEqual = allEqual && equal;
            print("  %-8s mediana %7.3f ms  p99 %7.3f ms  máx %7.3f ms  · %.0f subpasos/muestra · %zu tiles · hash %016llx %s\n",
                  impl->name, percentile(r.times, 0.5), percentile(r.times, 0.99), percentile(r.times, 1.0),
                  double(r.substeps) / double(r.times.size()), r.tiles, static_cast<unsigned long long>(r.hash),
                  equal ? "" : "<-- DISTINTO");
        }
        print("\n");
    }
    print(allEqual ? "Determinismo: todas las rutas dejan el mismo papel.\n"
                   : "FALLA: alguna ruta deja un papel distinto.\n");
    print("Resultado guardado en %s\n", file);
    if (g_out)
        fclose(g_out);
    if (pause) {
        printf("\nPresioná Enter para salir.");
        (void)getchar();
    }
    return allEqual ? 0 : 1;
}
