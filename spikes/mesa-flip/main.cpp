// Spike HU-81 (Mesa de animación, Fase 0): cuánto cuesta cambiar de hoja. Código descartable.
//
// Mide, con la hoja real (7680 × 4800 celdas, toda la tableta) y trazos reales grabados con
// `cartuchera --grabar`:
//   1. cuánto pesa una hoja dibujada (tiles de depósito tocados);
//   2. cambiar la hoja activa de la simulación copiando sus tiles afuera y adentro del papel;
//   3. redibujar la hoja entera desde la simulación (renderTone), por si el flip pudiera
//      redibujar en vez de guardar imágenes;
//   4. reconstruir la imagen desde un tono de 8 bits por píxel (la imagen chica);
//   5. subir la imagen a la GPU, y copiar entre texturas que ya están en la GPU.
//
// Uso: mesa-flip [muestras.csv]   (por defecto <datos>\cartuchera-muestras.csv). Medir con el
// preset release.

#include <drymedia/Medium.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>
#include <lienzo/SheetMapping.h>
#include <lienzo/Tone.h>

#include <windows.h>

#include <QByteArray>

#include <d3d11.h>
#include <psapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

constexpr double kTabletWidthMm = 325.1, kTabletHeightMm = 203.2;
constexpr int kClientW = 2560, kClientH = 1080;     // ultra-wide de la desktop
constexpr int kAreaW = 1734, kAreaH = 1081;         // área útil calibrada
constexpr int kSheets = 120;                        // RNF-02

std::vector<std::vector<drymedia::PencilSample>> loadStrokes(const std::string& path)
{
    std::ifstream in(path);
    std::string line;
    std::getline(in, line); // encabezado
    std::vector<std::vector<drymedia::PencilSample>> strokes;
    std::vector<drymedia::PencilSample> current;
    while (std::getline(in, line)) {
        std::vector<std::string> f;
        std::stringstream ss(line);
        for (std::string cell; std::getline(ss, cell, ',');)
            f.push_back(cell);
        if (f.size() < 10)
            continue;
        // timeUs,x,y,celdaX,celdaY,presion,azimut,altitud,contacto,goma,...
        const bool contact = f[8] == "1" && f[9] == "0";
        if (contact) {
            drymedia::PencilSample s;
            s.x = std::stod(f[3]);
            s.y = std::stod(f[4]);
            s.pressure = std::stof(f[5]);
            s.azimuth = std::stof(f[6]);
            s.altitude = std::stof(f[7]);
            current.push_back(s);
        } else if (!current.empty()) {
            strokes.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty())
        strokes.push_back(std::move(current));
    return strokes;
}

double ms(std::chrono::steady_clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

// Mediana y máximo de n corridas.
void measure(const char* name, int n, const std::function<void()>& work)
{
    std::vector<double> t;
    for (int i = 0; i < n; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        work();
        t.push_back(ms(std::chrono::steady_clock::now() - t0));
    }
    std::sort(t.begin(), t.end());
    std::printf("  %-58s mediana %8.3f ms   máx %8.3f ms\n", name, t[t.size() / 2], t.back());
}

double workingSetMb()
{
    PROCESS_MEMORY_COUNTERS pmc{};
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    return pmc.WorkingSetSize / 1048576.0;
}

// Los tiles de una hoja fuera del papel: índice, planos y contenido.
struct SheetTiles {
    std::vector<int> index;
    std::vector<uint8_t> planes;
    std::vector<uint16_t> data; // kTileStride por tile
    size_t bytes() const { return data.size() * 2 + index.size() * 5; }
};

SheetTiles saveTiles(const drymedia::Paper& paper)
{
    SheetTiles out;
    const int n = paper.tilesX() * paper.tilesY();
    for (int i = 0; i < n; ++i) {
        const int tx = i % paper.tilesX(), ty = i / paper.tilesX();
        if (const uint16_t* t = paper.findDepositTile(tx, ty)) {
            out.index.push_back(i);
            out.planes.push_back(paper.tilePlanes(tx, ty));
            out.data.insert(out.data.end(), t, t + drymedia::kTileStride);
        }
    }
    return out;
}

void loadTiles(drymedia::Paper& paper, const SheetTiles& tiles)
{
    paper.clear();
    for (size_t k = 0; k < tiles.index.size(); ++k) {
        const int i = tiles.index[k];
        uint16_t* t = paper.depositTile(i % paper.tilesX(), i / paper.tilesX());
        std::memcpy(t, tiles.data.data() + k * drymedia::kTileStride, drymedia::kTileStride * 2);
        paper.setTilePlanes(i % paper.tilesX(), i / paper.tilesX(), tiles.planes[k]);
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::string path;
    if (argc > 1) {
        path = argv[1];
    } else {
        char buf[MAX_PATH];
        GetEnvironmentVariableA("LOCALAPPDATA", buf, MAX_PATH);
        path = std::string(buf) + "\\trazos\\cartuchera-muestras.csv";
    }
    const auto strokes = loadStrokes(path);
    size_t samples = 0;
    for (const auto& s : strokes)
        samples += s.size();
    std::printf("Muestras: %s\n  %zu trazos, %zu muestras con contacto\n\n", path.c_str(), strokes.size(), samples);
    if (strokes.empty())
        return 1;

    const double ws0 = workingSetMb();
    auto t0 = std::chrono::steady_clock::now();
    drymedia::Paper paper({.seed = 1, .widthMm = kTabletWidthMm, .heightMm = kTabletHeightMm});
    std::printf("1. Papel %d × %d celdas (%d × %d tiles)\n", paper.width(), paper.height(), paper.tilesX(), paper.tilesY());
    std::printf("  crear el papel (relieve y crestas): %.0f ms, +%.0f MB de memoria\n", ms(std::chrono::steady_clock::now() - t0),
                workingSetMb() - ws0);

    // Dibuja los trazos grabados: una hoja de rough "real".
    t0 = std::chrono::steady_clock::now();
    {
        drymedia::Pencil pencil(paper, drymedia::Medium::hb());
        for (const auto& stroke : strokes) {
            pencil.beginStroke(stroke.front());
            for (size_t i = 1; i < stroke.size(); ++i)
                pencil.strokeTo(stroke[i]);
            pencil.endStroke();
        }
    }
    const SheetTiles sheet = saveTiles(paper);
    int extraPlanes = 0;
    for (uint8_t p : sheet.planes)
        extraPlanes += (p & ~1u) != 0;
    std::printf("  dibujar los trazos: %.0f ms\n", ms(std::chrono::steady_clock::now() - t0));
    std::printf("  tiles tocados: %zu de %d (%.1f %%), %zu con planos además del depósito\n", sheet.index.size(),
                paper.tilesX() * paper.tilesY(), 100.0 * sheet.index.size() / (paper.tilesX() * paper.tilesY()), size_t(extraPlanes));
    std::printf("  hoja guardada como tiles (4 planos): %.1f MB; solo depósito: %.1f MB\n", sheet.bytes() / 1048576.0,
                sheet.index.size() * drymedia::kTileCells * 2 / 1048576.0);
    std::printf("  %d hojas así: %.0f MB\n\n", kSheets, kSheets * sheet.bytes() / 1048576.0);

    // La grabación puede ser chica: se repiten los mismos trazos desplazados por la hoja
    // hasta llegar a coberturas de un rough real.
    std::printf("1b. Coberturas mayores (los mismos trazos, desplazados)\n");
    {
        double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        for (const auto& st : strokes)
            for (const auto& p : st) {
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
            }
        const double bw = std::max(200.0, maxX - minX), bh = std::max(200.0, maxY - minY);
        drymedia::Paper heavy({.seed = 1, .widthMm = kTabletWidthMm, .heightMm = kTabletHeightMm});
        drymedia::Pencil pencil(heavy, drymedia::Medium::hb());
        const int total = heavy.tilesX() * heavy.tilesY();
        uint32_t rng = 12345;
        const auto rnd = [&rng] {
            rng = rng * 1664525u + 1013904223u;
            return (rng >> 8) / double(1 << 24);
        };
        for (const double target : {0.10, 0.25, 0.50}) {
            while (double(heavy.tileCount()) / total < target) {
                const double dx = rnd() * (heavy.width() - bw) - minX, dy = rnd() * (heavy.height() - bh) - minY;
                const auto moved = [&](drymedia::PencilSample q) {
                    q.x += dx;
                    q.y += dy;
                    return q;
                };
                for (const auto& st : strokes) {
                    pencil.beginStroke(moved(st.front()));
                    for (size_t i = 1; i < st.size(); ++i)
                        pencil.strokeTo(moved(st[i]));
                    pencil.endStroke();
                }
            }
            const SheetTiles t = saveTiles(heavy);
            std::printf("  %2.0f %% de tiles: %5.1f MB por hoja, %5.0f MB las %d hojas\n", 100.0 * t.index.size() / total,
                        t.bytes() / 1048576.0, kSheets * t.bytes() / 1048576.0, kSheets);
            char name[96];
            std::snprintf(name, sizeof name, "cargar una hoja al %2.0f %% en la simulación", 100.0 * t.index.size() / total);
            measure(name, 10, [&] { loadTiles(paper, t); });
            // Comprimida (zlib de Qt): cuánto ocupa una hoja no activa en RAM o en disco.
            const QByteArray raw(reinterpret_cast<const char*>(t.data.data()), qsizetype(t.data.size() * 2));
            QByteArray packed;
            measure("  comprimir la hoja (qCompress nivel 1)", 3, [&] { packed = qCompress(raw, 1); });
            measure("  descomprimir la hoja", 5, [&] { (void)qUncompress(packed); });
            std::printf("    comprimida: %5.1f MB por hoja (%.0f : 1), %5.0f MB las %d hojas\n", packed.size() / 1048576.0,
                        double(raw.size()) / packed.size(), kSheets * packed.size() / 1048576.0, kSheets);
        }
        loadTiles(paper, sheet);
    }
    std::printf("\n");

    std::printf("2. Cambiar la hoja activa de la simulación (copiar tiles)\n");
    SheetTiles other = sheet; // otra hoja del mismo peso
    measure("guardar los tiles de la hoja que sale", 20, [&] { other = saveTiles(paper); });
    measure("cargar los tiles de la hoja que entra (clear + copia)", 20, [&] { loadTiles(paper, sheet); });
    std::printf("\n");

    lienzo::SheetMapping mapping = lienzo::SheetMapping::onTablet(kClientW, kClientH, 0, 0, kAreaW, kAreaH, kTabletWidthMm,
                                                                  kTabletHeightMm, kTabletWidthMm, kTabletHeightMm,
                                                                  paper.width(), paper.height());
    std::vector<uint32_t> image(size_t(kClientW) * kClientH);
    std::printf("3. Redibujar la hoja entera desde la simulación (hoja %d × %d px)\n", mapping.sheetWidth, mapping.sheetHeight);
    measure("renderTone de la hoja (un hilo)", 10, [&] {
        lienzo::renderTone(paper, mapping, mapping.sheetX, mapping.sheetY, mapping.sheetX + mapping.sheetWidth,
                           mapping.sheetY + mapping.sheetHeight, image.data());
    });
    std::printf("\n");

    const size_t sheetPixels = size_t(mapping.sheetWidth) * mapping.sheetHeight;
    std::printf("4. Imagen guardada por hoja (%.1f MB en BGRA, %.1f MB en tono de 8 bits)\n", sheetPixels * 4 / 1048576.0,
                sheetPixels / 1048576.0);
    std::vector<uint32_t> cached(sheetPixels);
    std::vector<uint8_t> tone(sheetPixels);
    for (int y = 0; y < mapping.sheetHeight; ++y)
        for (int x = 0; x < mapping.sheetWidth; ++x) {
            const uint32_t c = image[size_t(mapping.sheetY + y) * kClientW + mapping.sheetX + x];
            cached[size_t(y) * mapping.sheetWidth + x] = c;
            tone[size_t(y) * mapping.sheetWidth + x] = uint8_t(255 - ((c >> 8) & 0xFF)); // verde: 0 = hoja
        }
    uint32_t lut[256];
    for (int i = 0; i < 256; ++i)
        lut[i] = lienzo::toneOf(uint32_t(i) * 65535 / 255);
    std::vector<uint32_t> rebuilt(sheetPixels);
    measure("copiar la imagen BGRA guardada", 20, [&] { std::memcpy(rebuilt.data(), cached.data(), sheetPixels * 4); });
    measure("reconstruir BGRA desde el tono de 8 bits (tabla)", 20, [&] {
        for (size_t i = 0; i < sheetPixels; ++i)
            rebuilt[i] = lut[tone[i]];
    });
    std::printf("  %d hojas: %.0f MB en BGRA, %.0f MB en tono\n\n", kSheets, kSheets * sheetPixels * 4 / 1048576.0,
                kSheets * sheetPixels / 1048576.0);

    std::printf("5. GPU\n");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device,
                                 nullptr, &ctx))) {
        std::printf("  sin device D3D11\n");
        return 0;
    }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = UINT(mapping.sheetWidth);
    desc.Height = UINT(mapping.sheetHeight);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> target;
    device->CreateTexture2D(&desc, nullptr, &target);

    D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT, 0};
    ComPtr<ID3D11Query> done;
    device->CreateQuery(&qd, &done);
    const auto waitGpu = [&] {
        ctx->End(done.Get());
        while (ctx->GetData(done.Get(), nullptr, 0, 0) == S_FALSE)
            YieldProcessor();
    };
    measure("subir la imagen BGRA (UpdateSubresource) hasta que la GPU termina", 30, [&] {
        ctx->UpdateSubresource(target.Get(), 0, nullptr, cached.data(), UINT(mapping.sheetWidth * 4), 0);
        waitGpu();
    });

    // Todas las hojas viviendo en la GPU: el flip sería una copia entre texturas.
    std::vector<ComPtr<ID3D11Texture2D>> sheets;
    D3D11_SUBRESOURCE_DATA init{cached.data(), UINT(mapping.sheetWidth * 4), 0};
    for (int i = 0; i < kSheets; ++i) {
        ComPtr<ID3D11Texture2D> t;
        if (FAILED(device->CreateTexture2D(&desc, &init, &t)))
            break;
        sheets.push_back(t);
    }
    waitGpu();
    std::printf("  texturas creadas en la GPU: %zu de %d (%.0f MB)\n", sheets.size(), kSheets,
                sheets.size() * sheetPixels * 4 / 1048576.0);
    if (!sheets.empty()) {
        int k = 0;
        measure("copiar una hoja que ya está en la GPU (CopyResource)", 60, [&] {
            ctx->CopyResource(target.Get(), sheets[size_t(k++) % sheets.size()].Get());
            waitGpu();
        });
    }
    std::printf("\nMemoria del proceso al final: %.0f MB\n", workingSetMb());
    return 0;
}
