#include "lienzo/Lienzo.h"

#include "lienzo/Bench.h"
#include "lienzo/CanvasWindow.h"
#include "lienzo/DisplayImage.h"
#include "lienzo/Media.h"
#include "lienzo/Renderer.h"
#include "lienzo/SampleQueue.h"
#include "lienzo/Simulation.h"
#include "lienzo/Timing.h"

#include <appkit/Config.h>
#include <appkit/Paths.h>
#include <appkit/ScreenChoice.h>
#include <appkit/UsableArea.h>
#include <drymedia/Paper.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QPicture>
#include <QSaveFile>
#include <QScreen>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <optional>
#include <share.h>
#include <thread>

namespace lienzo {

namespace {

// Superficie activa de la Wacom Intuos4 Large (PTK-840), según el plan.
constexpr double kTabletWidthMm = 325.1;
constexpr double kTabletHeightMm = 203.2;

// medios.json (HU-57 y HU-58): las diez durezas y la goma. Se crea con los valores de
// fábrica, se recarga al editarlo a mano y Ctrl+S guarda ahí la herramienta activa (lo
// demás queda como estaba en el archivo).
struct MediaFile {
    QString path;
    MediaSet current;       // en uso, con lo calibrado en vivo
    MediaSet saved;         // lo que dice el archivo
    QByteArray lastWritten; // para no recargar lo propio
    std::atomic<int> active{kHbIndex};
    std::atomic<bool> leadUnsaved{false};   // la mina activa tiene cambios sin guardar
    std::atomic<bool> eraserUnsaved{false}; // la goma tiene cambios sin guardar

    const char* activeName() const { return kGradeNames[size_t(active.load())]; }
    Lead& lead() { return current.grades[size_t(active.load())]; }
    void updateUnsaved()
    {
        leadUnsaved = current.grades[size_t(active.load())] != saved.grades[size_t(active.load())];
        eraserUnsaved = current.eraser != saved.eraser;
    }

    bool write()
    {
        const QByteArray bytes = mediaToJson(saved);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            qWarning() << "No se pudo escribir" << path << file.errorString();
            return false;
        }
        lastWritten = bytes;
        return true;
    }

    // Lee el archivo; si no existe, lo crea con los valores de fábrica. Devuelve true si
    // cambiaron los valores en uso.
    bool load()
    {
        QFile file(path);
        if (!file.exists()) {
            qInfo() << "Creando" << path << "con los valores de fábrica";
            write();
            return false;
        }
        if (!file.open(QIODevice::ReadOnly))
            return false;
        const QByteArray bytes = file.readAll();
        if (bytes == lastWritten)
            return false; // lo acabamos de guardar nosotros
        MediaSet loaded;
        QString error;
        if (!mediaFromJson(bytes, loaded, &error)) {
            qWarning() << path << "no se entiende, sigo con los valores anteriores:" << error;
            return false;
        }
        lastWritten = bytes;
        current = saved = loaded;
        updateUnsaved();
        qInfo() << "Medios leídos de" << path;
        return true;
    }
};

} // namespace

void Shell::keyPressEvent(QKeyEvent* event)
{
    if (onKey)
        onKey(UINT(event->nativeVirtualKey()), event->modifiers().testFlag(Qt::ControlModifier));
}

QScreen* savedScreen(const appkit::Config& config)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (const auto saved =
            appkit::ScreenId::fromVariant(config.value(QStringLiteral("monitor"), {}, appkit::Config::Scope::Common))) {
        QList<appkit::ScreenId> ids;
        for (const QScreen* s : screens)
            ids.append(appkit::ScreenId::of(s));
        if (const auto index = appkit::findScreen(ids, *saved))
            return screens[*index];
    }
    return QGuiApplication::primaryScreen();
}

struct Lienzo::Impl {
    LienzoOptions options;
    std::optional<QRect> area;
    drymedia::Paper paper; // A4 apaisado recortado, 297 × 203 mm
    SampleQueue queue;
    MediaFile media;
    std::function<void(UINT, bool)> appKeys;
    std::unique_ptr<CanvasWindow> canvas;
    SheetMapping mapping;
    DisplayImage image;
    std::unique_ptr<Simulation> sim;
    std::unique_ptr<Renderer> render;
    std::unique_ptr<QFileSystemWatcher> watcher;
    SessionTimings timings;
    std::FILE* recording = nullptr;
    std::thread bench;
    bool running = false;

    // La mina activa y la goma (con lo calibrado en vivo) pasan a la simulación.
    void applyMedia()
    {
        media.updateUnsaved();
        if (sim) {
            sim->setLead(media.lead());
            sim->setEraser(media.current.eraser);
        }
    }

    // Calibración en vivo: cambia un valor de la herramienta activa.
    void adjust(int& value, bool up, double factor, int minimum, int maximum)
    {
        const int next = up ? std::max(value + 1, int(std::lround(value * factor))) : int(std::lround(value / factor));
        value = std::clamp(next, minimum, maximum);
        applyMedia();
    }

    // Las teclas de calibración y Ctrl+S van a la goma mientras el lápiz está dado vuelta.
    bool erasing() const { return sim && sim->erasing(); }

    void saveScreenImage()
    {
        QImage copy;
        {
            std::lock_guard lock(image.mutex);
            copy = QImage(reinterpret_cast<const uchar*>(image.pixels.data()), image.width, image.height,
                          QImage::Format_RGB32)
                       .copy();
        }
        const QString path = QDir(appkit::familyDataDirectory())
                                 .filePath(QStringLiteral("%1-%2.png")
                                               .arg(options.name,
                                                    QDateTime::currentDateTime().toString(QStringLiteral("HHmmss"))));
        qInfo() << "Imagen de pantalla guardada en" << path << copy.save(path);
    }

    std::wstring overlayText() const
    {
        wchar_t text[512];
        const wchar_t* warning =
            area ? L"" : L"\nSin área calibrada en este monitor: calibrala con F9 en Ejercicios.";
        if (sim->erasing()) {
            const Eraser eraser = sim->eraser();
            const bool unsaved = media.eraserUnsaved;
            swprintf(text, 512, L"goma%ls   ·   fuerza %d ([ ])   ·   %.1f mm (, .)%ls%ls", unsaved ? L"*" : L"",
                     eraser.strength, eraser.diameter / 100.0, unsaved ? L"   ·   Ctrl+S guarda" : L"", warning);
        } else {
            const Lead lead = sim->lead();
            const bool unsaved = media.leadUnsaved;
            swprintf(text, 512,
                     options.gradeKeys ? L"mina %hs%ls (1-0)   ·   blandura %d ([ ])   ·   %.2f mm (, .)   ·   techo %d %% (- =)%ls%ls"
                                       : L"mina %hs%ls   ·   blandura %d ([ ])   ·   %.2f mm (, .)   ·   techo %d %% (- =)%ls%ls",
                     media.activeName(), unsaved ? L"*" : L"", lead.softness, lead.diameter / 100.0,
                     int(std::lround(lead.ceiling * 100.0 / 65535)), unsaved ? L"   ·   Ctrl+S guarda" : L"", warning);
        }
        return text;
    }
};

Lienzo::Lienzo(Shell& shell, QScreen* screen, const appkit::Config& config, LienzoOptions options)
    : d(std::make_unique<Impl>())
{
    d->options = std::move(options);
    // Área útil: la de la familia de apps (sección común de config.json), la misma que se
    // calibra con F9.
    d->area = appkit::loadUsableArea(config, appkit::ScreenId::of(screen));

    d->media.path = QDir(appkit::familyDataDirectory()).filePath(QStringLiteral("medios.json"));
    d->media.load();

    const auto dispatch = [this](UINT vk, bool ctrl) {
        if (!handleKey(vk, ctrl) && d->appKeys)
            d->appKeys(vk, ctrl);
    };
    shell.onKey = dispatch;
    d->canvas = std::make_unique<CanvasWindow>(reinterpret_cast<HWND>(shell.winId()), d->queue, dispatch);

    // Hoja = tableta (HU-52): el área útil corresponde a toda la superficie activa, y la hoja
    // va centrada encima, a escala real. Sin área calibrada, ajustada a la ventana.
    const drymedia::Paper& paper = d->paper;
    const auto& area = d->area;
    d->mapping = area ? SheetMapping::onTablet(d->canvas->width(), d->canvas->height(), area->x(), area->y(),
                                               area->width(), area->height(), kTabletWidthMm, kTabletHeightMm,
                                               paper.spec().widthMm, paper.spec().heightMm, paper.width(),
                                               paper.height())
                      : SheetMapping::fit(d->canvas->width(), d->canvas->height(), paper.width(), paper.height());

    d->image.width = d->canvas->width();
    d->image.height = d->canvas->height();
    d->image.pixels.resize(size_t(d->image.width) * size_t(d->image.height));

    d->sim = std::make_unique<Simulation>(d->paper, d->queue, d->image, d->mapping);
    d->sim->setUndo(d->options.undo);
    d->applyMedia();
    d->sim->renderAll();
    d->render = std::make_unique<Renderer>(d->canvas->hwnd(), d->image);
    d->render->setExtraInfo([impl = d.get()] { return impl->overlayText(); });
    if (!area)
        d->render->setOverlay(true); // el aviso tiene que verse sin apretar nada

    // Recarga en caliente de medios.json. Muchos editores guardan reemplazando el archivo y
    // el watcher lo pierde: se vuelve a agregar.
    d->watcher = std::make_unique<QFileSystemWatcher>(QStringList{d->media.path});
    QObject::connect(d->watcher.get(), &QFileSystemWatcher::fileChanged, [impl = d.get()] {
        if (!impl->watcher->files().contains(impl->media.path) && QFile::exists(impl->media.path))
            impl->watcher->addPath(impl->media.path);
        if (impl->media.load())
            impl->applyMedia();
    });

    qInfo() << "Monitor" << appkit::ScreenId::of(screen).describe() << (area ? "· área útil" : "· SIN área útil")
            << (area ? *area : QRect());
    qInfo() << "Hoja" << paper.width() << "x" << paper.height() << "celdas en" << d->mapping.sheetWidth << "x"
            << d->mapping.sheetHeight << "px desde" << d->mapping.sheetX << d->mapping.sheetY << "("
            << d->mapping.pixelsPerCellX << "x" << d->mapping.pixelsPerCellY << "px por celda)";
}

Lienzo::~Lienzo()
{
    stop();
}

void Lienzo::setAppKeys(std::function<void(UINT, bool)> keys)
{
    d->appKeys = std::move(keys);
}

void Lienzo::setOnStylusButton(std::function<void()> callback)
{
    d->canvas->setOnStylusButton(std::move(callback));
}

bool Lienzo::handleKey(UINT vk, bool ctrl)
{
    MediaFile& media = d->media;
    if (ctrl && vk == 'N')
        clear();
    else if (!ctrl && vk == 'Z' && d->options.undo) // Z sola (pedido del usuario, 10 de octubre)
        d->sim->requestUndo();
    else if (ctrl && vk == 'Y' && d->options.undo)
        d->sim->requestRedo();
    else if (vk == VK_F3)
        d->render->toggleOverlay();
    else if (vk == VK_F12) // diagnóstico: guarda la imagen de pantalla
        d->saveScreenImage();
    else if (ctrl && vk == 'S') {
        if (d->erasing()) {
            media.saved.eraser = media.current.eraser;
            if (media.write())
                qInfo() << "Guardada la goma en" << media.path;
        } else {
            const size_t a = size_t(media.active.load());
            media.saved.grades[a] = media.current.grades[a];
            if (media.write())
                qInfo() << "Guardada la" << media.activeName() << "en" << media.path;
        }
        media.updateUnsaved();
    } else if (!ctrl && vk >= '0' && vk <= '9' && d->options.gradeKeys) { // 1 a 0: 2H … 6B
        media.active = vk == '0' ? kGradeCount - 1 : int(vk - '1');
        d->applyMedia();
    } else if (vk == VK_OEM_4 || vk == VK_OEM_6) { // [ y ] blandura o fuerza de la goma, pasos de ~25 %
        int& value = d->erasing() ? media.current.eraser.strength : media.lead().softness;
        d->adjust(value, vk == VK_OEM_6, 1.25, 1, 255);
    } else if (vk == VK_OEM_COMMA || vk == VK_OEM_PERIOD) { // , y . diámetro, pasos de ~15 %
        if (d->erasing()) // goma: 2 a 8 mm
            d->adjust(media.current.eraser.diameter, vk == VK_OEM_PERIOD, 1.15, 200, 800);
        else // mina: 0,30 a 2,00 mm (punta de 48 celdas)
            d->adjust(media.lead().diameter, vk == VK_OEM_PERIOD, 1.15, 30, 200);
    } else if ((vk == VK_OEM_MINUS || vk == VK_OEM_PLUS) && !d->erasing()) // - y = techo de tono, pasos de ~10 %
        d->adjust(media.lead().ceiling, vk == VK_OEM_PLUS, 1.1, 2000, 65535);
    else
        return false;
    return true;
}

void Lienzo::clear()
{
    d->sim->requestClear();
}

void Lienzo::start()
{
    if (d->running)
        return;
    d->running = true;
    const QStringList args = QApplication::arguments();
    d->sim->setTimings(&d->timings);
    d->render->setTimings(&d->timings);
    // --grabar: las muestras crudas a <datos>/<nombre>-muestras.csv (diagnóstico; se
    // re-simulan con tst_pencil y DRYMEDIA_REPLAY).
    if (args.contains(QStringLiteral("--grabar"))) {
        const QString path =
            QDir(appkit::familyDataDirectory()).filePath(QStringLiteral("%1-muestras.csv").arg(d->options.name));
        d->recording = _wfsopen(reinterpret_cast<const wchar_t*>(path.utf16()), L"w", _SH_DENYNO);
        if (d->recording) {
            std::fputs("timeUs,x,y,celdaX,celdaY,presion,azimut,altitud,contacto,goma,blandura,diametro,techo\n",
                       d->recording);
            d->sim->setRecording(d->recording);
            qInfo() << "Grabando muestras en" << path;
        }
    }
    d->sim->start();
    d->render->start();
    // --bench [undo]: trazos sintéticos durante 30 s, para medir sin la tableta.
    if (args.contains(QStringLiteral("--bench"))) {
        const bool withUndo = args.contains(QStringLiteral("undo")) && d->options.undo;
        qInfo() << "Benchmark sintético" << (withUndo ? "con deshacer" : "sin deshacer");
        d->bench = std::thread([impl = d.get(), withUndo] {
            runBench(impl->queue, *impl->sim, impl->mapping, withUndo);
        });
    }
}

void Lienzo::stop()
{
    if (!d->running)
        return;
    d->running = false;
    if (d->bench.joinable())
        d->bench.join();
    d->sim->stop();
    d->render->stop();
    if (d->recording) {
        std::fclose(d->recording);
        d->recording = nullptr;
    }
    const Lead lead = d->sim->lead();
    qInfo().noquote() << QString::fromStdString(d->render->summary()) << "· mina final" << d->media.activeName()
                      << "· blandura" << lead.softness << "·" << lead.diameter / 100.0 << "mm · techo" << lead.ceiling;
    SessionTimings& t = d->timings;
    for (const auto& [name, stat] : {std::pair{"sim lote", &t.simBatch}, {"sim lápiz", &t.simPencil},
                                     {"sim candado tomado", &t.simLockHeld}, {"sim deshacer", &t.simUndo},
                                     {"render toma → Present", &t.renderLatchToPresent}})
        qInfo().noquote() << QString::fromStdString(stat->describe(name));
    qInfo().noquote() << QString::fromStdString(
        t.renderLockBusy.describe("render con la imagen ocupada (1 = no la tomó)", "frames"));
    qInfo().noquote() << QString::fromStdString(t.renderUploadPixels.describe("render píxeles subidos", "px"));
}

void Lienzo::setGuides(const QPicture& guides)
{
    std::vector<uint32_t> layer;
    if (!guides.isNull()) {
        QImage image(d->image.width, d->image.height, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipRect(sheetRect());
        painter.translate(d->mapping.sheetX, d->mapping.sheetY);
        painter.drawPicture(0, 0, guides);
        painter.end();
        const auto* bits = reinterpret_cast<const uint32_t*>(image.constBits());
        layer.assign(bits, bits + size_t(image.width()) * size_t(image.height()));
    }
    d->sim->setGuides(std::move(layer));
}

QRect Lienzo::sheetRect() const
{
    return {d->mapping.sheetX, d->mapping.sheetY, d->mapping.sheetWidth, d->mapping.sheetHeight};
}

const SheetMapping& Lienzo::mapping() const
{
    return d->mapping;
}

bool Lienzo::calibrated() const
{
    return d->area.has_value();
}

} // namespace lienzo
