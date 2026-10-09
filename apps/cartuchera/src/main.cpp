// Cartuchera: dibujo con medios secos (docs/medios-secos/arquitectura.md). Grafito de
// 2H a 6B sobre una hoja A4, sin interfaz.
//   Alt+F4 sale · Ctrl+N hoja nueva · Ctrl+Z deshace y Ctrl+Y rehace (hasta 100 trazos) ·
//   1 a 0 eligen la dureza (2H … 6B) · F3 latencia y mina en vivo · calibración de la mina
//   activa: [ y ] blandura, , y . diámetro, - y = techo de tono; Ctrl+S la guarda en
//   medios.json (que se recarga solo si se edita a mano)
//   Diagnóstico: --grabar guarda las muestras en <datos>/cartuchera-muestras.csv (se
//   re-simulan con tst_pencil y DRYMEDIA_REPLAY) · F12 guarda la imagen de pantalla

#include "CanvasWindow.h"
#include "DisplayImage.h"
#include "Renderer.h"
#include "SampleQueue.h"
#include "SheetMapping.h"
#include "Bench.h"
#include "Media.h"
#include "Simulation.h"
#include "Timing.h"

#include <appkit/Config.h>
#include <appkit/Log.h>
#include <appkit/Paths.h>
#include <appkit/ScreenChoice.h>
#include <appkit/UsableArea.h>
#include <drymedia/Paper.h>

#include <QApplication>
#include <QDateTime>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QSaveFile>
#include <QKeyEvent>
#include <QScreen>
#include <QWidget>

#include <algorithm>
#include <cstdio>
#include <share.h>
#include <cmath>
#include <functional>
#include <thread>

namespace {

// Cascarón Qt: sin bordes, cubre el monitor principal. Las teclas que llegan acá (cuando
// Qt tiene el foco) van al mismo manejador que las del lienzo.
class Shell : public QWidget {
public:
    std::function<void(UINT, bool)> onKey;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (onKey)
            onKey(UINT(event->nativeVirtualKey()), event->modifiers().testFlag(Qt::ControlModifier));
    }
};

// Superficie activa de la Wacom Intuos4 Large (PTK-840), según el plan.
constexpr double kTabletWidthMm = 325.1;
constexpr double kTabletHeightMm = 203.2;

// El monitor guardado ("monitor", sección común) si está conectado; si no, el principal.
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

// medios.json (HU-57): las diez durezas. Se crea con los valores de fábrica, se recarga
// al editarlo a mano y Ctrl+S guarda ahí la mina activa (las demás quedan como estaban
// en el archivo).
struct MediaFile {
    QString path;
    cartuchera::Grades grades = cartuchera::factoryGrades(); // en uso, con lo calibrado en vivo
    cartuchera::Grades saved = grades;                      // lo que dice el archivo
    QByteArray lastWritten;                                 // para no recargar lo propio
    std::atomic<int> active{cartuchera::kHbIndex};
    std::atomic<bool> unsaved{false}; // la mina activa tiene cambios sin guardar

    const char* activeName() const { return cartuchera::kGradeNames[size_t(active.load())]; }
    void updateUnsaved() { unsaved = grades[size_t(active.load())] != saved[size_t(active.load())]; }

    bool write()
    {
        const QByteArray bytes = cartuchera::gradesToJson(saved);
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
        cartuchera::Grades loaded;
        QString error;
        if (!cartuchera::gradesFromJson(bytes, loaded, &error)) {
            qWarning() << path << "no se entiende, sigo con los valores anteriores:" << error;
            return false;
        }
        lastWritten = bytes;
        grades = saved = loaded;
        updateUnsaved();
        qInfo() << "Medios leídos de" << path;
        return true;
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Cartuchera"));
    appkit::installFileLog(QStringLiteral("cartuchera"));

    // Monitor y área útil: los de la familia de apps (sección común de config.json), los
    // mismos que se eligen y calibran en Ejercicios (F10 y F9).
    appkit::Config config(QStringLiteral("cartuchera"));
    QScreen* screen = savedScreen(config);
    const std::optional<QRect> area = appkit::loadUsableArea(config, appkit::ScreenId::of(screen));

    Shell shell;
    shell.setWindowFlag(Qt::FramelessWindowHint);
    shell.setWindowTitle(QApplication::applicationName());
    shell.setScreen(screen); // antes de crear la ventana nativa, o Windows elige el monitor
    shell.setGeometry(screen->geometry());
    shell.show();

    drymedia::Paper paper; // A4 apaisado recortado, 297 × 203 mm
    cartuchera::SampleQueue queue;
    cartuchera::Simulation* simulation = nullptr;
    cartuchera::Renderer* renderer = nullptr;
    cartuchera::DisplayImage* displayImage = nullptr;

    MediaFile media;
    media.path = QDir(appkit::familyDataDirectory()).filePath(QStringLiteral("medios.json"));
    media.load();
    // La mina activa (con lo calibrado en vivo) pasa a la simulación.
    const auto applyLead = [&] {
        media.updateUnsaved();
        if (simulation)
            simulation->setLead(media.grades[size_t(media.active.load())]);
    };
    // Calibración en vivo: cambia un valor de la mina activa.
    const auto adjust = [&](int cartuchera::Lead::*field, bool up, double factor, int minimum, int maximum) {
        int& value = media.grades[size_t(media.active.load())].*field;
        const int next = up ? std::max(value + 1, int(std::lround(value * factor))) : int(std::lround(value / factor));
        value = std::clamp(next, minimum, maximum);
        applyLead();
    };

    const auto onKey = [&](UINT vk, bool ctrl) {
        if (ctrl && vk == 'N' && simulation)
            simulation->requestClear();
        else if (ctrl && vk == 'Z' && simulation)
            simulation->requestUndo();
        else if (ctrl && vk == 'Y' && simulation)
            simulation->requestRedo();
        else if (vk == VK_F3 && renderer)
            renderer->toggleOverlay();
        else if (vk == VK_F12 && displayImage) { // diagnóstico: guarda la imagen de pantalla
            QImage copy;
            {
                std::lock_guard lock(displayImage->mutex);
                copy = QImage(reinterpret_cast<const uchar*>(displayImage->pixels.data()), displayImage->width,
                              displayImage->height, QImage::Format_RGB32)
                           .copy();
            }
            const QString path = QDir(appkit::familyDataDirectory())
                                     .filePath(QStringLiteral("cartuchera-%1.png")
                                                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("HHmmss"))));
            qInfo() << "Imagen de pantalla guardada en" << path << copy.save(path);
        }
        else if (ctrl && vk == 'S') {
            const size_t a = size_t(media.active.load());
            media.saved[a] = media.grades[a];
            if (media.write())
                qInfo() << "Guardada la" << media.activeName() << "en" << media.path;
            media.updateUnsaved();
        } else if (!ctrl && vk >= '0' && vk <= '9') { // 1 a 0: 2H … 6B
            media.active = vk == '0' ? cartuchera::kGradeCount - 1 : int(vk - '1');
            applyLead();
        } else if (vk == VK_OEM_4 || vk == VK_OEM_6) // [ y ] blandura, pasos de ~25 %
            adjust(&cartuchera::Lead::softness, vk == VK_OEM_6, 1.25, 1, 255);
        else if (vk == VK_OEM_COMMA || vk == VK_OEM_PERIOD) // , y . diámetro, 0,30 a 2,00 mm (punta de 48 celdas)
            adjust(&cartuchera::Lead::diameter, vk == VK_OEM_PERIOD, 1.15, 30, 200);
        else if (vk == VK_OEM_MINUS || vk == VK_OEM_PLUS) // - y = techo de tono, pasos de ~10 %
            adjust(&cartuchera::Lead::ceiling, vk == VK_OEM_PLUS, 1.1, 2000, 65535);
    };
    shell.onKey = onKey;

    cartuchera::CanvasWindow canvas(reinterpret_cast<HWND>(shell.winId()), queue, onKey);
    // Hoja = tableta (HU-52): el área útil corresponde a toda la superficie activa, y la
    // hoja va centrada encima, a escala real. Sin área calibrada, ajustada a la ventana.
    const cartuchera::SheetMapping mapping =
        area ? cartuchera::SheetMapping::onTablet(canvas.width(), canvas.height(), area->x(), area->y(), area->width(),
                                                  area->height(), kTabletWidthMm, kTabletHeightMm,
                                                  paper.spec().widthMm, paper.spec().heightMm, paper.width(),
                                                  paper.height())
             : cartuchera::SheetMapping::fit(canvas.width(), canvas.height(), paper.width(), paper.height());

    cartuchera::DisplayImage image;
    image.width = canvas.width();
    image.height = canvas.height();
    image.pixels.resize(size_t(image.width) * size_t(image.height));
    displayImage = &image;

    cartuchera::Simulation sim(paper, queue, image, mapping);
    sim.setLead(media.grades[size_t(media.active.load())]);
    sim.renderAll();
    cartuchera::Renderer render(canvas.hwnd(), image);
    const bool calibrated = area.has_value();
    render.setExtraInfo([&sim, &media, calibrated] {
        const cartuchera::Lead lead = sim.lead();
        wchar_t text[512];
        swprintf(text, 512,
                 L"mina %hs%ls (1-0)   ·   blandura %d ([ ])   ·   %.2f mm (, .)   ·   techo %d %% (- =)%ls%ls",
                 media.activeName(), media.unsaved ? L"*" : L"", lead.softness, lead.diameter / 100.0,
                 int(std::lround(lead.ceiling * 100.0 / 65535)), media.unsaved ? L"   ·   Ctrl+S guarda" : L"",
                 calibrated ? L"" : L"\nSin área calibrada en este monitor: calibrala con F9 en Ejercicios.");
        return std::wstring(text);
    });
    if (!calibrated)
        render.setOverlay(true); // el aviso tiene que verse sin apretar nada
    simulation = &sim;
    renderer = &render;
    // Recarga en caliente de medios.json. Muchos editores guardan reemplazando el archivo y
    // el watcher lo pierde: se vuelve a agregar.
    QFileSystemWatcher watcher({media.path});
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, [&] {
        if (!watcher.files().contains(media.path) && QFile::exists(media.path))
            watcher.addPath(media.path);
        if (media.load())
            applyLead();
    });
    qInfo() << "Monitor" << appkit::ScreenId::of(screen).describe() << (calibrated ? "· área útil" : "· SIN área útil")
            << (area ? *area : QRect());
    qInfo() << "Hoja" << paper.width() << "x" << paper.height() << "celdas en" << mapping.sheetWidth << "x"
            << mapping.sheetHeight << "px desde" << mapping.sheetX << mapping.sheetY << "(" << mapping.pixelsPerCellX
            << "x" << mapping.pixelsPerCellY << "px por celda)";

    cartuchera::SessionTimings timings;
    sim.setTimings(&timings);
    // --grabar: las muestras crudas a <datos>/cartuchera-muestras.csv (diagnóstico).
    std::FILE* recording = nullptr;
    if (QApplication::arguments().contains(QStringLiteral("--grabar"))) {
        const QString path = QDir(appkit::familyDataDirectory()).filePath(QStringLiteral("cartuchera-muestras.csv"));
        recording = _wfsopen(reinterpret_cast<const wchar_t*>(path.utf16()), L"w", _SH_DENYNO);
        if (recording) {
            std::fputs("timeUs,x,y,celdaX,celdaY,presion,azimut,altitud,contacto,goma,blandura,diametro,techo\n",
                       recording);
            sim.setRecording(recording);
            qInfo() << "Grabando muestras en" << path;
        }
    }
    render.setTimings(&timings);

    sim.start();
    render.start();
    // --bench [undo]: trazos sintéticos durante 30 s, para medir sin la tableta.
    const QStringList args = QApplication::arguments();
    std::thread bench;
    if (args.contains(QStringLiteral("--bench"))) {
        const bool withUndo = args.contains(QStringLiteral("undo"));
        qInfo() << "Benchmark sintético" << (withUndo ? "con deshacer" : "sin deshacer");
        bench = std::thread([&queue, &sim, mapping, withUndo] { cartuchera::runBench(queue, sim, mapping, withUndo); });
    }
    const int result = QApplication::exec();
    if (bench.joinable())
        bench.join();
    sim.stop();
    render.stop();
    if (recording)
        std::fclose(recording);
    qInfo().noquote() << QString::fromStdString(render.summary()) << "· mina final" << media.activeName()
                      << "· blandura" << sim.lead().softness << "·" << sim.lead().diameter / 100.0 << "mm · techo"
                      << sim.lead().ceiling;
    for (const auto& [name, stat] : {std::pair{"sim lote", &timings.simBatch}, {"sim lápiz", &timings.simPencil},
                                     {"sim candado tomado", &timings.simLockHeld}, {"sim deshacer", &timings.simUndo},
                                     {"render toma → Present", &timings.renderLatchToPresent}})
        qInfo().noquote() << QString::fromStdString(stat->describe(name));
    qInfo().noquote() << QString::fromStdString(
        timings.renderLockBusy.describe("render con la imagen ocupada (1 = no la tomó)", "frames"));
    qInfo().noquote() << QString::fromStdString(timings.renderUploadPixels.describe("render píxeles subidos", "px"));
    return result;
}
