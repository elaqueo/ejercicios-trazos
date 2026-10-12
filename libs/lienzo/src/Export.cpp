#include "lienzo/Export.h"

#include "lienzo/SheetMapping.h"
#include "lienzo/Tone.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <thread>

namespace lienzo {

namespace {

const QString kIndexFile = QStringLiteral("toma.json");
// Cuántas hojas se comprimen a PNG a la vez mientras se dibuja la siguiente (cada una ocupa
// ~34 MB a 300 dpi; comprimir tarda más que dibujar).
constexpr size_t kEncoders = 3;

// La hoja entera, un píxel por celda.
SheetMapping cellMapping(const drymedia::Paper& paper)
{
    SheetMapping m;
    m.clientWidth = m.cellsWidth = m.sheetWidth = paper.width();
    m.clientHeight = m.cellsHeight = m.sheetHeight = paper.height();
    return m;
}

void renderCells(const drymedia::Paper& paper, const SheetMapping& m, uint32_t* pixels, uint32_t paperColor)
{
    const int threads = int(std::clamp(std::thread::hardware_concurrency(), 1u, 8u));
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        const int y0 = m.sheetHeight * t / threads, y1 = m.sheetHeight * (t + 1) / threads;
        pool.emplace_back([&, y0, y1] { renderTone(paper, m, 0, y0, m.sheetWidth, y1, pixels, nullptr, nullptr, paperColor); });
    }
    for (std::thread& th : pool)
        th.join();
}

} // namespace

QString exportFileName(int number)
{
    return number > 0 ? QStringLiteral("%1.png").arg(number, 4, 10, QLatin1Char('0')) : QStringLiteral("fija.png");
}

ExportResult exportTake(const ExportJob& job, const std::function<void(int done, int total)>& progress,
                        const std::atomic<bool>* cancel)
{
    ExportResult result;
    QDir folder(job.dir);
    if (!folder.mkpath(QStringLiteral("."))) {
        result.error = QStringLiteral("no se pudo crear la carpeta %1").arg(job.dir);
        return result;
    }
    // Lo de una exportación anterior (solo lo que escribe esta: no toca otros archivos).
    for (const QString& name : folder.entryList({QStringLiteral("*.png"), kIndexFile}, QDir::Files))
        folder.remove(name);

    drymedia::Paper paper(job.paper);
    const SheetMapping m = cellMapping(paper);
    const int total = int(job.sheets.size());
    std::mutex mutex;
    QString encodeError;
    std::deque<std::thread> encoders;
    int done = 0;
    const auto finishOne = [&] {
        encoders.front().join();
        encoders.pop_front();
        if (progress)
            progress(++done, total);
    };
    QJsonArray list;
    QString fixedFile;
    for (const ExportSheet& sheet : job.sheets) {
        if (cancel && *cancel) {
            result.error = QStringLiteral("cancelada");
            break;
        }
        drymedia::TileSet tiles;
        if (!paper.deserializeTiles(sheet.state.data(), sheet.state.size(), tiles)) {
            result.error = QStringLiteral("la hoja %1 no se pudo leer").arg(exportFileName(sheet.number));
            break;
        }
        paper.swapTiles(tiles);
        QImage image(m.sheetWidth, m.sheetHeight, QImage::Format_RGB32); // BGRA en memoria, como Tone.h
        renderCells(paper, m, reinterpret_cast<uint32_t*>(image.bits()), job.paperColor);
        // Puntos por metro, para que otra app la abra a su tamaño real.
        const int dpm = int(std::lround(job.paper.dpi / 0.0254));
        image.setDotsPerMeterX(dpm);
        image.setDotsPerMeterY(dpm);

        const QString name = exportFileName(sheet.number);
        if (sheet.number > 0) {
            list.append(QJsonObject{{QStringLiteral("archivo"), name},
                                    {QStringLiteral("numero"), sheet.number},
                                    {QStringLiteral("rol"), roleName(sheet.role)}});
            ++result.sheets;
        } else {
            fixedFile = name;
        }
        if (encoders.size() >= kEncoders)
            finishOne();
        encoders.emplace_back([&, image = std::move(image), path = folder.filePath(name), name] {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
                std::lock_guard lock(mutex);
                if (encodeError.isEmpty())
                    encodeError = QStringLiteral("%1: %2").arg(name, file.errorString());
            }
        });
    }
    while (!encoders.empty())
        finishOne();
    if (result.error.isEmpty())
        result.error = encodeError;
    if (!result.error.isEmpty())
        return result;

    // El audio, copiado con el mismo nombre, y el índice.
    QJsonObject audio;
    if (!job.audio.isEmpty()) {
        const QString name = QFileInfo(job.audio).fileName();
        folder.remove(name);
        if (!QFile::copy(job.audio, folder.filePath(name))) {
            result.error = QStringLiteral("no se pudo copiar el audio %1").arg(name);
            return result;
        }
        QJsonArray marks;
        for (const double t : job.marks)
            marks.append(t);
        audio = {{QStringLiteral("archivo"), name}, {QStringLiteral("marcas"), marks}};
    }
    QJsonObject root{{QStringLiteral("formato"), QStringLiteral("exportación de Mesa de animación")},
                     {QStringLiteral("version"), 1},
                     {QStringLiteral("dpi"), job.paper.dpi},
                     {QStringLiteral("ancho"), m.sheetWidth},
                     {QStringLiteral("alto"), m.sheetHeight},
                     {QStringLiteral("hojas"), list}};
    if (!fixedFile.isEmpty())
        root.insert(QStringLiteral("fija"), fixedFile);
    if (!audio.isEmpty())
        root.insert(QStringLiteral("audio"), audio);
    QSaveFile file(folder.filePath(kIndexFile));
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson()) < 0 || !file.commit()) {
        result.error = QStringLiteral("toma.json: %1").arg(file.errorString());
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace lienzo
