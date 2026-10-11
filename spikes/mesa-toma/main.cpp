// Spike HU-85 (Mesa de animación): qué compresión sin pérdida usar para las hojas guardadas en
// disco. Código descartable.
//
// Con la hoja real de Mesa de animación (toda la tableta, 300 dpi, grano 300 desde HU-113; antes 600 y 900, medio
// recortado: solo el depósito) y trazos reales grabados con `cartuchera --grabar`, repetidos
// por la hoja hasta un 10, 26 y 50 % de tiles:
//   1. el estado de la hoja (índice de tiles + depósito) con zlib (Qt), zstd y lz4, con y sin
//      separar los bytes altos y bajos de los u16: tamaño, tiempo de comprimir y de
//      descomprimir (un hilo);
//   2. la imagen de pantalla de la hoja: guardarla (comprimida o PNG) o redibujarla al abrir;
//   3. escribir y leer el archivo (disco).
//
// Uso: mesa-toma [muestras.csv]   (por defecto <datos>\trazos\cartuchera-muestras.csv).

#include <drymedia/Medium.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>
#include <lienzo/Media.h>
#include <lienzo/SheetMapping.h>
#include <lienzo/Tone.h>

#include <windows.h>
#include <io.h>

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QImage>

#include <lz4.h>
#include <lz4hc.h>
#include <zstd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr double kTabletWidthMm = 325.1, kTabletHeightMm = 203.2;
constexpr int kAreaW = 1734, kAreaH = 1081; // la hoja en la pantalla de la desktop
constexpr int kSheets = 120;                // RNF-02
constexpr int kCores = 4;                   // i5 Skylake de la desktop

using Bytes = std::vector<char>;

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
        if (f[8] == "1" && f[9] == "0") {
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

double median(int n, const std::function<void()>& work)
{
    std::vector<double> t;
    for (int i = 0; i < n; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        work();
        t.push_back(ms(std::chrono::steady_clock::now() - t0));
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

// El estado de la hoja como iría al archivo: cantidad de tiles, índice (u32) y planos (u8) de
// cada uno, y después los planos que usa cada tile (el depósito y los marcados), seguidos.
// `headerBytes` separa el índice de los datos u16 (lo único que se baraja).
struct SheetState {
    Bytes bytes;
    size_t headerBytes = 0;
};

SheetState serialize(const drymedia::Paper& paper)
{
    std::vector<uint32_t> index;
    std::vector<uint8_t> planes;
    std::vector<uint16_t> data;
    const int n = paper.tilesX() * paper.tilesY();
    for (int i = 0; i < n; ++i) {
        const int tx = i % paper.tilesX(), ty = i / paper.tilesX();
        const uint16_t* t = paper.findDepositTile(tx, ty);
        if (!t)
            continue;
        const uint8_t p = paper.tilePlanes(tx, ty);
        index.push_back(uint32_t(i));
        planes.push_back(p);
        for (int plane = 0; plane < drymedia::kTilePlanes; ++plane)
            if (plane == drymedia::kDepositPlane || (p & (1u << plane)))
                data.insert(data.end(), t + plane * drymedia::kTileCells, t + (plane + 1) * drymedia::kTileCells);
    }
    SheetState s;
    const uint32_t count = uint32_t(index.size());
    s.bytes.resize(4 + index.size() * 4 + planes.size() + data.size() * 2);
    char* w = s.bytes.data();
    std::memcpy(w, &count, 4);
    std::memcpy(w + 4, index.data(), index.size() * 4);
    std::memcpy(w + 4 + index.size() * 4, planes.data(), planes.size());
    s.headerBytes = 4 + index.size() * 4 + planes.size();
    std::memcpy(w + s.headerBytes, data.data(), data.size() * 2);
    return s;
}

// Separa los bytes bajos y altos de los u16 de los datos (el índice queda igual).
Bytes shuffle(const SheetState& s)
{
    Bytes out(s.bytes.size());
    std::memcpy(out.data(), s.bytes.data(), s.headerBytes);
    const size_t n = (s.bytes.size() - s.headerBytes) / 2;
    const char* src = s.bytes.data() + s.headerBytes;
    char* lo = out.data() + s.headerBytes;
    char* hi = lo + n;
    for (size_t i = 0; i < n; ++i) {
        lo[i] = src[2 * i];
        hi[i] = src[2 * i + 1];
    }
    return out;
}

void unshuffle(const Bytes& in, size_t headerBytes, Bytes& out)
{
    out.resize(in.size());
    std::memcpy(out.data(), in.data(), headerBytes);
    const size_t n = (in.size() - headerBytes) / 2;
    const char* lo = in.data() + headerBytes;
    const char* hi = lo + n;
    char* dst = out.data() + headerBytes;
    for (size_t i = 0; i < n; ++i) {
        dst[2 * i] = lo[i];
        dst[2 * i + 1] = hi[i];
    }
}

struct Codec {
    const char* name;
    std::function<Bytes(const Bytes&)> compress;
    std::function<void(const Bytes& packed, Bytes& out)> decompress; // out ya tiene el tamaño
};

std::vector<Codec> codecs()
{
    std::vector<Codec> list;
    for (const int level : {1, 6}) {
        list.push_back({level == 1 ? "zlib 1" : "zlib 6",
                        [level](const Bytes& in) {
                            const QByteArray c = qCompress(reinterpret_cast<const uchar*>(in.data()), qsizetype(in.size()), level);
                            return Bytes(c.begin(), c.end());
                        },
                        [](const Bytes& p, Bytes& out) {
                            const QByteArray u = qUncompress(reinterpret_cast<const uchar*>(p.data()), qsizetype(p.size()));
                            std::memcpy(out.data(), u.data(), std::min(out.size(), size_t(u.size())));
                        }});
    }
    for (const int level : {1, 3, 9, 19}) {
        static const char* names[] = {"zstd 1", "zstd 3", "zstd 9", "zstd 19"};
        const char* name = names[level == 1 ? 0 : level == 3 ? 1 : level == 9 ? 2 : 3];
        list.push_back({name,
                        [level](const Bytes& in) {
                            Bytes out(ZSTD_compressBound(in.size()));
                            out.resize(ZSTD_compress(out.data(), out.size(), in.data(), in.size(), level));
                            return out;
                        },
                        [](const Bytes& p, Bytes& out) { ZSTD_decompress(out.data(), out.size(), p.data(), p.size()); }});
    }
    list.push_back({"lz4",
                    [](const Bytes& in) {
                        Bytes out(size_t(LZ4_compressBound(int(in.size()))));
                        out.resize(size_t(LZ4_compress_default(in.data(), out.data(), int(in.size()), int(out.size()))));
                        return out;
                    },
                    [](const Bytes& p, Bytes& out) { LZ4_decompress_safe(p.data(), out.data(), int(p.size()), int(out.size())); }});
    list.push_back({"lz4 HC 9",
                    [](const Bytes& in) {
                        Bytes out(size_t(LZ4_compressBound(int(in.size()))));
                        out.resize(size_t(LZ4_compress_HC(in.data(), out.data(), int(in.size()), int(out.size()), 9)));
                        return out;
                    },
                    [](const Bytes& p, Bytes& out) { LZ4_decompress_safe(p.data(), out.data(), int(p.size()), int(out.size())); }});
    return list;
}

double mb(size_t bytes)
{
    return bytes / 1048576.0;
}

void compareState(const SheetState& s, const std::vector<Codec>& list)
{
    std::printf("  %-22s %8s %7s %10s %9s %13s %14s\n", "", "MB", "razón", "guardar", "abrir", "120 hojas MB",
                "abrir 120 (s)");
    std::printf("  %-22s %8.1f %7s %10s %9s %13.0f %14s\n", "sin comprimir", mb(s.bytes.size()), "1 : 1", "-", "-",
                kSheets * mb(s.bytes.size()), "-");
    const Bytes shuffled = shuffle(s);
    for (const bool sh : {false, true}) {
        for (const Codec& c : list) {
            const Bytes& input = sh ? shuffled : s.bytes;
            Bytes packed;
            // Guardar = barajar (si va) + comprimir; abrir = descomprimir + desbarajar.
            const double save = median(3, [&] { packed = sh ? c.compress(shuffle(s)) : c.compress(input); });
            Bytes out(s.bytes.size()), tmp(s.bytes.size());
            const double open = median(5, [&] {
                if (sh) {
                    c.decompress(packed, tmp);
                    unshuffle(tmp, s.headerBytes, out);
                } else {
                    c.decompress(packed, out);
                }
            });
            const bool same = out == s.bytes;
            char name[48];
            std::snprintf(name, sizeof name, "%s%s", c.name, sh ? " + barajado" : "");
            std::printf("  %-22s %8.2f %5.1f:1 %7.0f ms %6.1f ms %13.0f %14.1f%s\n", name, mb(packed.size()),
                        double(s.bytes.size()) / packed.size(), save, open, kSheets * mb(packed.size()),
                        kSheets * open / kCores / 1000.0, same ? "" : "   ¡NO ES IGUAL!");
        }
    }
}

void renderParallel(const drymedia::Paper& paper, const lienzo::SheetMapping& m, std::vector<uint32_t>& image, int threads,
                    uint32_t paperColor)
{
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        const int y0 = kAreaH * t / threads, y1 = kAreaH * (t + 1) / threads;
        pool.emplace_back([&, y0, y1] { lienzo::renderTone(paper, m, 0, y0, kAreaW, y1, image.data(), nullptr, nullptr, paperColor); });
    }
    for (auto& th : pool)
        th.join();
}

void compareImage(const drymedia::Paper& paper, const std::vector<Codec>& list)
{
    constexpr uint32_t paperColor = lienzo::bgra(0xFA, 0xF8, 0xF2);
    const lienzo::SheetMapping m = lienzo::SheetMapping::fit(kAreaW, kAreaH, paper.width(), paper.height());
    std::vector<uint32_t> image(size_t(kAreaW) * kAreaH);
    const double one = median(3, [&] { lienzo::renderTone(paper, m, 0, 0, kAreaW, kAreaH, image.data(), nullptr, nullptr, paperColor); });
    const double many = median(5, [&] { renderParallel(paper, m, image, kCores, paperColor); });
    std::printf("  redibujarla al abrir (renderTone): %.0f ms en un hilo, %.0f ms en %d; abrir 120: %.1f s\n", one, many, kCores,
                kSheets * one / kCores / 1000.0);
    const Bytes raw(reinterpret_cast<const char*>(image.data()), reinterpret_cast<const char*>(image.data() + image.size()));
    std::printf("  guardada: %.1f MB sin comprimir\n", mb(raw.size()));
    for (const Codec& c : list) {
        if (std::strcmp(c.name, "zstd 19") == 0 || std::strcmp(c.name, "zlib 6") == 0)
            continue;
        Bytes packed, out(raw.size());
        const double save = median(3, [&] { packed = c.compress(raw); });
        const double open = median(5, [&] { c.decompress(packed, out); });
        std::printf("  %-22s %8.2f MB %5.0f:1 %7.0f ms %6.1f ms%s\n", c.name, mb(packed.size()), double(raw.size()) / packed.size(),
                    save, open, out == raw ? "" : "   ¡NO ES IGUAL!");
    }
    const QImage img(reinterpret_cast<const uchar*>(image.data()), kAreaW, kAreaH, QImage::Format_RGB32);
    QByteArray png;
    const double save = median(3, [&] {
        png.clear();
        QBuffer b(&png);
        (void)b.open(QIODevice::WriteOnly);
        img.save(&b, "PNG");
    });
    QImage back;
    const double open = median(5, [&] { back = QImage::fromData(png, "PNG"); });
    std::printf("  %-22s %8.2f MB %5.0f:1 %7.0f ms %6.1f ms\n", "PNG (Qt)", mb(size_t(png.size())), double(raw.size()) / png.size(), save,
                open);
}

void compareDisk(const SheetState& s)
{
    const Bytes shuffled = shuffle(s);
    Bytes packed(ZSTD_compressBound(shuffled.size()));
    packed.resize(ZSTD_compress(packed.data(), packed.size(), shuffled.data(), shuffled.size(), 3));
    const QString path = QDir::temp().filePath(QStringLiteral("mesa-toma-hoja.bin"));
    const double write = median(5, [&] {
        QFile f(path);
        (void)f.open(QIODevice::WriteOnly);
        f.write(packed.data(), qsizetype(packed.size()));
        f.flush();
        FlushFileBuffers(HANDLE(_get_osfhandle(f.handle())));
    });
    QByteArray readBack;
    const double read = median(5, [&] {
        QFile f(path);
        (void)f.open(QIODevice::ReadOnly);
        readBack = f.readAll();
    });
    QFile::remove(path);
    std::printf("  escribir la hoja (zstd 3 + barajado, %.1f MB) hasta el disco: %.1f ms; leerla (en caché del sistema): %.1f ms\n",
                mb(packed.size()), write, read);
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
    std::printf("Muestras: %s\n  %zu trazos, %zu muestras con contacto\n", path.c_str(), strokes.size(), samples);
    if (strokes.empty())
        return 1;

    // La hoja de Mesa de animación: toda la tableta, 300 dpi, grano 300 (HU-113), HB sin funciones de
    // ilustración (HU-97, HU-98, HU-112).
    drymedia::Paper paper({.seed = 1, .widthMm = kTabletWidthMm, .heightMm = kTabletHeightMm, .dpi = 300, .grain = 300});
    const drymedia::Medium hb =
        lienzo::forProfile(lienzo::factoryGrades()[lienzo::kHbIndex].medium(), lienzo::MediumProfile::Animation);
    drymedia::Pencil pencil(paper, hb);
    std::printf("Papel %d × %d celdas (%d × %d tiles)\n\n", paper.width(), paper.height(), paper.tilesX(), paper.tilesY());

    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (const auto& st : strokes)
        for (const auto& p : st) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    const double bw = std::max(200.0, maxX - minX), bh = std::max(200.0, maxY - minY);
    const int total = paper.tilesX() * paper.tilesY();
    uint32_t rng = 12345;
    const auto rnd = [&rng] {
        rng = rng * 1664525u + 1013904223u;
        return (rng >> 8) / double(1 << 24);
    };
    const std::vector<Codec> list = codecs();
    for (const double target : {0.10, 0.26, 0.50}) {
        while (double(paper.tileCount()) / total < target) {
            const double dx = rnd() * (paper.width() - bw) - minX, dy = rnd() * (paper.height() - bh) - minY;
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
        const SheetState s = serialize(paper);
        int extra = 0;
        for (int i = 0; i < total; ++i)
            extra += paper.findDepositTile(i % paper.tilesX(), i / paper.tilesX()) && paper.tilePlanes(i % paper.tilesX(), i / paper.tilesX()) != 0;
        std::printf("1. Estado de la hoja al %.0f %% de tiles (%zu tiles, %d con planos además del depósito)\n",
                    100.0 * paper.tileCount() / total, paper.tileCount(), extra);
        compareState(s, list);
        if (target == 0.26) {
            std::printf("\n2. Imagen de pantalla de la hoja al 26 %% (%d × %d, BGRA)\n", kAreaW, kAreaH);
            compareImage(paper, list);
            std::printf("\n3. Disco\n");
            compareDisk(s);
        }
        std::printf("\n");
    }
    return 0;
}
