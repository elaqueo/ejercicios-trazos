#include "lienzo/Take.h"

#include "lienzo/SheetStack.h"

#include "Zstd.h"

#include <drymedia/Paper.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <set>
#include <thread>

namespace lienzo {

namespace {

const QString kTakeFile = QStringLiteral("toma.json");
constexpr char kMagic[4] = {'T', 'R', 'Z', 'H'};
constexpr uint16_t kSheetVersion = 1;
constexpr uint8_t kCodecZstd = 1;

// Cabecera del archivo de hoja, little endian, sin relleno.
#pragma pack(push, 1)
struct SheetHeader {
    char magic[4];
    uint16_t version;
    uint8_t codec;
    uint8_t reserved;
    uint32_t seed, dpi, grain, widthCells, heightCells; // el papel
    uint64_t stateSize, statePacked;                    // depósito: sin comprimir y comprimido
    uint32_t imageWidth, imageHeight;                   // imagen de pantalla (0 × 0: no hay)
    uint64_t imagePacked;
};
#pragma pack(pop)
static_assert(sizeof(SheetHeader) == 60);

QString sheetFileName(uint64_t id)
{
    return QStringLiteral("hoja-%1.hoja").arg(id);
}

// Corre work(i) para i en [0, n), repartido entre los núcleos.
void parallelFor(int n, const std::function<void(int)>& work)
{
    const int threads = std::clamp(int(std::thread::hardware_concurrency()), 1, std::clamp(n, 1, 8));
    std::atomic<int> next{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([&] {
            for (int i = next++; i < n; i = next++)
                work(i);
        });
    for (std::thread& th : pool)
        th.join();
}

QJsonObject paperJson(const drymedia::Paper& paper)
{
    const drymedia::PaperSpec& s = paper.spec();
    return {{QStringLiteral("semilla"), qint64(s.seed)},
            {QStringLiteral("dpi"), s.dpi},
            {QStringLiteral("grano"), s.grain},
            {QStringLiteral("anchoCeldas"), paper.width()},
            {QStringLiteral("altoCeldas"), paper.height()}};
}

// Escribe el archivo de una hoja; devuelve el error, o vacío.
QString writeSheet(const QString& path, const drymedia::Paper& paper, const std::vector<uint8_t>& state,
                   const PackedImage& image)
{
    const std::vector<uint8_t> packed = zstd::compress(state.data(), state.size());
    if (packed.empty())
        return QStringLiteral("no se pudo comprimir");
    SheetHeader h{};
    std::memcpy(h.magic, kMagic, 4);
    h.version = kSheetVersion;
    h.codec = kCodecZstd;
    h.seed = paper.spec().seed;
    h.dpi = uint32_t(paper.spec().dpi);
    h.grain = uint32_t(paper.spec().grain);
    h.widthCells = uint32_t(paper.width());
    h.heightCells = uint32_t(paper.height());
    h.stateSize = state.size();
    h.statePacked = packed.size();
    if (!image.empty()) {
        h.imageWidth = uint32_t(image.width);
        h.imageHeight = uint32_t(image.height);
        h.imagePacked = image.bytes.size();
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return file.errorString();
    file.write(reinterpret_cast<const char*>(&h), sizeof(h));
    file.write(reinterpret_cast<const char*>(packed.data()), qint64(packed.size()));
    if (!image.empty())
        file.write(reinterpret_cast<const char*>(image.bytes.data()), qint64(image.bytes.size()));
    return file.commit() ? QString() : file.errorString();
}

// Lee el archivo de una hoja; devuelve el error, o vacío.
QString readSheet(const QString& path, const drymedia::Paper& paper, drymedia::TileSet& tiles, PackedImage& image)
{
    QFile file(path);
    if (!file.exists())
        return QStringLiteral("falta el archivo");
    if (!file.open(QIODevice::ReadOnly))
        return file.errorString();
    const QByteArray bytes = file.readAll();
    SheetHeader h{};
    if (size_t(bytes.size()) < sizeof(h))
        return QStringLiteral("archivo cortado");
    std::memcpy(&h, bytes.constData(), sizeof(h));
    if (std::memcmp(h.magic, kMagic, 4) != 0)
        return QStringLiteral("no es un archivo de hoja");
    if (h.version > kSheetVersion || h.codec != kCodecZstd)
        return QStringLiteral("versión %1 o compresión %2 desconocida").arg(h.version).arg(h.codec);
    if (h.seed != paper.spec().seed || h.dpi != uint32_t(paper.spec().dpi) || h.grain != uint32_t(paper.spec().grain) ||
        h.widthCells != uint32_t(paper.width()) || h.heightCells != uint32_t(paper.height()))
        return QStringLiteral("es de otro papel");
    const uint64_t available = uint64_t(bytes.size()) - sizeof(h);
    // El depósito no puede ser más grande que la hoja entera con todos sus planos.
    const uint64_t maxState =
        12 + uint64_t(paper.tilesX()) * uint64_t(paper.tilesY()) * (5 + uint64_t(drymedia::kTileStride) * 2);
    if (h.statePacked > available || h.imagePacked > available - h.statePacked || h.stateSize > maxState)
        return QStringLiteral("archivo cortado");
    const auto* body = reinterpret_cast<const uint8_t*>(bytes.constData()) + sizeof(h);
    std::vector<uint8_t> state(h.stateSize);
    if (!zstd::decompress(body, h.statePacked, state.data(), state.size()))
        return QStringLiteral("depósito dañado");
    if (!paper.deserializeTiles(state.data(), state.size(), tiles))
        return QStringLiteral("depósito inválido");
    image = {};
    if (h.imagePacked && h.imageWidth && h.imageHeight) {
        image.width = int(h.imageWidth);
        image.height = int(h.imageHeight);
        image.bytes.assign(body + h.statePacked, body + h.statePacked + h.imagePacked);
    }
    return {};
}

} // namespace

QString writeSheetFile(const QString& dir, const drymedia::Paper& paper, uint64_t id, const std::vector<uint8_t>& state,
                       const PackedImage& image)
{
    QDir folder(dir);
    if (!folder.mkpath(QStringLiteral(".")))
        return QStringLiteral("no se pudo crear la carpeta %1").arg(dir);
    return writeSheet(folder.filePath(sheetFileName(id)), paper, state, image);
}

QString writeTakeIndex(const QString& dir, const drymedia::Paper& paper, const std::vector<uint64_t>& ids, int active,
                       const TakeView& view)
{
    QDir folder(dir);
    if (!folder.mkpath(QStringLiteral(".")))
        return QStringLiteral("no se pudo crear la carpeta %1").arg(dir);
    QJsonArray list;
    std::set<QString> files;
    for (const uint64_t id : ids) {
        files.insert(sheetFileName(id));
        QJsonObject sheet{{QStringLiteral("id"), qint64(id)},
                          {QStringLiteral("archivo"), sheetFileName(id)},
                          {QStringLiteral("rol"), roleName(roleOf(view.roles, id))}}; // HU-104
        if (id == view.fixed) // HU-89
            sheet.insert(QStringLiteral("fija"), true);
        list.append(sheet);
    }
    QJsonArray marks; // HU-108
    for (const double t : view.marks)
        marks.append(t);
    const QJsonObject root{
        {QStringLiteral("formato"), QStringLiteral("toma de Mesa de animación")},
        {QStringLiteral("version"), kTakeVersion},
        {QStringLiteral("papel"), paperJson(paper)},
        {QStringLiteral("hojas"), list},
        {QStringLiteral("activa"), active},
        {QStringLiteral("circulo"), view.circle}, // HU-94
        {QStringLiteral("vista"), QJsonObject{{QStringLiteral("giro"), view.degrees},
                                              {QStringLiteral("zoom"), view.zoom},
                                              {QStringLiteral("pan"), QJsonArray{view.panX, view.panY}}}},
        // HU-106 a HU-108.
        {QStringLiteral("audio"),
         QJsonObject{{QStringLiteral("archivo"), view.audio}, {QStringLiteral("marcas"), marks}}},
    };
    QSaveFile file(folder.filePath(kTakeFile));
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson()) < 0 || !file.commit())
        return QStringLiteral("toma.json: %1").arg(file.errorString());
    // Las hojas borradas.
    for (const QString& name : folder.entryList({QStringLiteral("hoja-*.hoja")}, QDir::Files))
        if (!files.count(name))
            folder.remove(name);
    return {};
}

QString takeFolder(const QString& dir, const std::function<bool(const QString&)>& open)
{
    if (open(dir) || !QFileInfo::exists(QDir(dir).filePath(kTakeFile)))
        return dir;
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1-%2").arg(dir).arg(n);
        if (!QFileInfo::exists(candidate) || open(candidate))
            return candidate;
    }
}

TakeResult saveTake(const QString& dir, const drymedia::Paper& paper, const SheetStack& sheets, const TakeView& view,
                    const std::function<PackedImage(uint64_t sheet)>& image)
{
    TakeResult result;
    if (!QDir(dir).mkpath(QStringLiteral("."))) {
        result.error = QStringLiteral("no se pudo crear la carpeta %1").arg(dir);
        return result;
    }
    const SheetOrder& order = sheets.order();
    std::vector<QString> errors(size_t(order.count));
    parallelFor(order.count, [&](int i) {
        const uint64_t id = order.ids[size_t(i)];
        errors[size_t(i)] = writeSheetFile(dir, paper, id, sheets.serialize(paper, i), image(id));
    });
    for (int i = 0; i < order.count; ++i)
        if (!errors[size_t(i)].isEmpty()) {
            result.error = QStringLiteral("hoja %1: %2").arg(i + 1).arg(errors[size_t(i)]);
            return result;
        }
    result.error = writeTakeIndex(dir, paper, order.ids, order.active, view);
    result.ok = result.error.isEmpty();
    return result;
}

TakeResult openTake(const QString& dir, drymedia::Paper& paper, SheetStack& sheets, TakeView& view,
                    std::vector<std::pair<uint64_t, PackedImage>>* images)
{
    TakeResult result;
    const QDir folder(dir);
    QFile file(folder.filePath(kTakeFile));
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("no hay toma en %1").arg(dir);
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.error = QStringLiteral("toma.json dañado: %1").arg(parseError.errorString());
        return result;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() > kTakeVersion) {
        result.error = QStringLiteral("la toma es de una versión más nueva de la app");
        return result;
    }
    if (root.value(QStringLiteral("papel")).toObject() != paperJson(paper)) {
        result.error = QStringLiteral("la toma es de otro papel");
        return result;
    }
    struct Entry {
        uint64_t id = 0;
        QString file;
        drymedia::TileSet tiles;
        PackedImage image;
        QString error;
    };
    std::vector<Entry> entries;
    std::set<uint64_t> seen;
    view.roles.clear();
    view.fixed = 0;
    view.circle = root.value(QStringLiteral("circulo")).toBool(); // HU-94: las de antes, sin círculo
    // HU-106: solo el nombre, como las hojas: el audio está en la carpeta de la toma.
    view.audio = QFileInfo(root.value(QStringLiteral("audio")).toObject().value(QStringLiteral("archivo")).toString()).fileName();
    view.marks.clear(); // HU-108: en orden, sin negativos
    for (const QJsonValue v : root.value(QStringLiteral("audio")).toObject().value(QStringLiteral("marcas")).toArray())
        if (v.isDouble() && v.toDouble() >= 0)
            view.marks.push_back(v.toDouble());
    std::sort(view.marks.begin(), view.marks.end());
    for (const QJsonValue v : root.value(QStringLiteral("hojas")).toArray()) {
        const QJsonObject o = v.toObject();
        const qint64 id = o.value(QStringLiteral("id")).toInteger();
        if (id <= 0 || !seen.insert(uint64_t(id)).second)
            continue; // id inválido o repetido: esa hoja no existe
        Entry e;
        e.id = uint64_t(id);
        if (const SheetRole role = parseRole(o.value(QStringLiteral("rol")).toString()); role != SheetRole::None)
            view.roles[e.id] = role;
        if (o.value(QStringLiteral("fija")).toBool() && !view.fixed) // HU-89: solo una
            view.fixed = e.id;
        // Solo el nombre: los archivos de la toma están en su carpeta.
        e.file = QFileInfo(o.value(QStringLiteral("archivo")).toString()).fileName();
        entries.push_back(std::move(e));
    }
    if (entries.empty()) {
        result.error = QStringLiteral("la toma no tiene hojas");
        return result;
    }
    parallelFor(int(entries.size()), [&](int i) {
        Entry& e = entries[size_t(i)];
        e.error = e.file.isEmpty() ? QStringLiteral("sin archivo")
                                   : readSheet(folder.filePath(e.file), paper, e.tiles, e.image);
        if (!e.error.isEmpty())
            e.tiles = paper.newTileSet(); // en blanco
    });
    std::vector<std::pair<uint64_t, drymedia::TileSet>> stack;
    for (size_t i = 0; i < entries.size(); ++i) {
        Entry& e = entries[i];
        if (!e.error.isEmpty()) {
            result.blank.emplace_back(int(i), e.error);
            qWarning().noquote() << QStringLiteral("Toma: la hoja %1 (%2) queda en blanco: %3").arg(i + 1).arg(e.file, e.error);
        } else if (images && !e.image.empty()) {
            images->emplace_back(e.id, std::move(e.image));
        }
        stack.emplace_back(e.id, std::move(e.tiles));
    }
    const int active = std::clamp(root.value(QStringLiteral("activa")).toInt(), 0, int(stack.size()) - 1);
    sheets.replace(paper, std::move(stack), active);
    const QJsonObject vista = root.value(QStringLiteral("vista")).toObject();
    view.degrees = vista.value(QStringLiteral("giro")).toDouble();
    view.zoom = vista.value(QStringLiteral("zoom")).toDouble(1.0); // las tomas de antes: 100 %
    view.panX = vista.value(QStringLiteral("pan")).toArray().at(0).toDouble();
    view.panY = vista.value(QStringLiteral("pan")).toArray().at(1).toDouble();
    result.ok = true;
    return result;
}

} // namespace lienzo
