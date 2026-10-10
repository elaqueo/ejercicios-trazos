#include "lienzo/Lienzo.h"

#include "lienzo/Bench.h"
#include "lienzo/CanvasWindow.h"
#include "lienzo/DisplayImage.h"
#include "lienzo/LeadPicker.h"
#include "lienzo/Media.h"
#include "lienzo/Renderer.h"
#include "lienzo/SampleQueue.h"
#include "lienzo/SheetStack.h"
#include "lienzo/Simulation.h"
#include "lienzo/Timing.h"
#include "lienzo/ToolPage.h"

#include <appkit/CalibrationOverlay.h>
#include <appkit/Config.h>
#include <appkit/Paths.h>
#include <appkit/ScreenChoice.h>
#include <appkit/Shortcuts.h>
#include <appkit/ShortcutsOverlay.h>
#include <appkit/SidePanel.h>
#include <appkit/UsableArea.h>
#include <drymedia/Paper.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPainter>
#include <QPicture>
#include <QProcess>
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
    Shell* shell = nullptr;
    QScreen* screen = nullptr;
    appkit::Config* config = nullptr;
    QPicture guides;                          // las últimas guías (se reubican al calibrar)
    double viewDegrees = 0;                   // rotación de la vista (HU-40)
    bool tilt = true;                         // costado (HU-73)
    int texture = 20;                         // textura del papel en % (HU-75)
    double gestureStartDegrees = 0, gestureStartPointer = 0;
    bool ringTurned = false; // la rueda giró la vista desde que se apoyó el dedo (HU-74)
    std::function<void()> onSheetChanged;
    std::unique_ptr<QWidget> calibrationHost; // F9 (HU-66): ventana propia que cubre el monitor
    appkit::CalibrationOverlay* calibration = nullptr;
    std::unique_ptr<LeadPicker> picker; // F5 (HU-67)
    std::unique_ptr<appkit::ShortcutsOverlay> shortcutsOverlay; // Ctrl+, (HU-78)
    std::optional<QRect> area;
    // La hoja ocupa todo el mapeo de la tableta (HU-69, pedido del usuario: dibuja sobre una
    // A3 que cubre la superficie), así que coincide con el área útil calibrada.
    drymedia::Paper paper{{.widthMm = kTabletWidthMm, .heightMm = kTabletHeightMm}};
    // Pila de hojas (HU-82): la cambia solo la simulación; acá, junto al papel, sobrevive a
    // recrearla (F9). `order` es lo mismo visto desde la interfaz (sheetCount, activeSheet).
    SheetStack sheets{Simulation::kUndoLimit};
    SheetOrder order;
    SampleQueue queue;
    MediaFile media;
    appkit::Shortcuts shortcuts;
    std::function<void()> refreshPanel; // el panel (HU-12) vuelve a leer mina, goma y pantalla
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
    bool argumentsHandled = false;

    // Hoja = tableta (HU-52): el área útil corresponde a toda la superficie activa, y la hoja
    // va centrada encima, a escala real. Sin área calibrada, ajustada a la ventana.
    void computeMapping()
    {
        area = appkit::loadUsableArea(*config, appkit::ScreenId::of(screen));
        mapping = area ? SheetMapping::onTablet(canvas->width(), canvas->height(), area->x(), area->y(), area->width(),
                                                area->height(), kTabletWidthMm, kTabletHeightMm, paper.spec().widthMm,
                                                paper.spec().heightMm, paper.width(), paper.height())
                       : SheetMapping::fit(canvas->width(), canvas->height(), paper.width(), paper.height());
    }

    // Las guías a una capa del tamaño de la imagen, en el lugar de la hoja.
    std::vector<uint32_t> guideLayer() const
    {
        std::vector<uint32_t> layer;
        if (guides.isNull())
            return layer;
        QImage layerImage(image.width, image.height, QImage::Format_ARGB32_Premultiplied);
        layerImage.fill(Qt::transparent);
        QPainter painter(&layerImage);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipRect(mapping.sheetX, mapping.sheetY, mapping.sheetWidth, mapping.sheetHeight);
        painter.translate(mapping.sheetX, mapping.sheetY);
        painter.drawPicture(0, 0, guides);
        painter.end();
        const auto* bits = reinterpret_cast<const uint32_t*>(layerImage.constBits());
        layer.assign(bits, bits + size_t(layerImage.width()) * size_t(layerImage.height()));
        return layer;
    }

    // La simulación usa una copia del mapeo: si cambia la hoja, se arma otra (el papel, con lo
    // dibujado, queda; el historial de deshacer se pierde).
    void createSimulation()
    {
        sim = std::make_unique<Simulation>(paper, sheets, queue, image, mapping);
        sim->setUndo(options.undo);
        sim->setTimings(&timings);
        if (recording)
            sim->setRecording(recording);
        applyMedia();
        sim->setTilt(tilt);
        sim->setTexture(texture);
        sim->setGuides(guideLayer());
        sim->setRotation(rotation());
        if (render)
            render->setRotation(rotation());
        sim->renderAll();
    }

    RECT sheetRect() const
    {
        return {mapping.sheetX, mapping.sheetY, mapping.sheetX + mapping.sheetWidth, mapping.sheetY + mapping.sheetHeight};
    }

    // Mesa de luz y flip (HU-83): lo pedido por posición, pasado a ids para el render.
    std::vector<Lienzo::Underlay> underlays;
    int shownSheet = -1;
    void applyLayers()
    {
        if (!render)
            return;
        std::vector<SheetLayer> below;
        for (const Lienzo::Underlay& u : underlays)
            if (order.valid(u.index) && u.index != order.active)
                below.push_back({order.ids[size_t(u.index)], u.color, u.opacity});
        const uint64_t shown = order.valid(shownSheet) && shownSheet != order.active ? order.ids[size_t(shownSheet)] : 0;
        render->setLayers(shown, std::move(below));
    }

    ViewRotation rotation() const
    {
        return {viewDegrees, mapping.sheetX + mapping.sheetWidth / 2.0, mapping.sheetY + mapping.sheetHeight / 2.0};
    }

    // persist: guarda el giro en config.json (HU-13); el gesto guarda solo al soltar.
    void setViewRotation(double degrees, bool persist = true)
    {
        viewDegrees = ViewRotation::normalized(degrees);
        const ViewRotation r = rotation();
        sim->setRotation(r);
        if (render)
            render->setRotation(r);
        if (persist)
            config->setValue(QStringLiteral("viewRotation"), viewDegrees);
    }

    // Otra mina activa (F5 o 1 a 0); queda guardada para la próxima vez (HU-13).
    void setActive(int grade)
    {
        media.active = grade;
        applyMedia();
        config->setValue(QStringLiteral("lead"), QString::fromLatin1(media.activeName()));
    }

    // Lo guardado en config.json (HU-13): la mina activa, el giro de la vista, el costado y la
    // textura (común a la familia). Va antes de
    // crear la simulación, que los toma.
    void restoreState()
    {
        const QString lead = config->value(QStringLiteral("lead")).toString();
        for (int g = 0; g < kGradeCount; ++g)
            if (lead == QLatin1String(kGradeNames[size_t(g)]))
                media.active = g;
        viewDegrees = ViewRotation::normalized(config->value(QStringLiteral("viewRotation"), 0.0).toDouble());
        tilt = config->value(QStringLiteral("tilt"), true).toBool();
        texture = std::clamp(config->value(QStringLiteral("paperTexture"), 20, appkit::Config::Scope::Common).toInt(), 0, 100);
    }

    // Textura del papel (HU-75): de a 10 %, común a las apps de la familia.
    void stepTexture(bool up)
    {
        texture = std::clamp(texture + (up ? 10 : -10), 0, 100);
        sim->setTexture(texture);
        config->setValue(QStringLiteral("paperTexture"), texture, appkit::Config::Scope::Common);
        qInfo() << "Textura del papel:" << texture << "%";
    }

    // Afilar (HU-62): la mina activa vuelve a la punta cónica nueva.
    void sharpen()
    {
        sim->requestSharpen();
        qInfo() << "Afilada:" << media.activeName();
    }

    // Punta seca (HU-61): no se guarda; al abrir la app siempre está la mina.
    void toggleStylus()
    {
        sim->setStylus(!sim->stylus());
        qInfo() << "Punta seca:" << (sim->stylus() ? "activada" : "desactivada");
    }

    // Costado (HU-73): apagado, la punta es siempre la vertical; queda guardado.
    void toggleTilt()
    {
        tilt = !tilt;
        sim->setTilt(tilt);
        config->setValue(QStringLiteral("tilt"), tilt);
        qInfo() << "Costado:" << (tilt ? "activado" : "desactivado");
    }

    double pointerAngle(double x, double y) const
    {
        const ViewRotation r = rotation();
        return std::atan2(y - r.cy, x - r.cx) * 180.0 / 3.14159265358979323846;
    }

    void rotateGesture(int phase, double x, double y)
    {
        if (phase == 0) {
            gestureStartDegrees = viewDegrees;
            gestureStartPointer = pointerAngle(x, y);
            return;
        }
        // Gira según el ángulo que recorre la punta alrededor del centro de la hoja, libre
        // (HU-79; antes con snap de 15°).
        setViewRotation(ViewRotation::dragged(gestureStartDegrees, gestureStartPointer, pointerAngle(x, y)), phase == 2);
        if (phase == 2)
            qInfo() << "Vista rotada" << viewDegrees << "°";
    }

    void relayout()
    {
        const bool wasRunning = running;
        if (wasRunning)
            sim->stop();
        computeMapping();
        createSimulation();
        render->setSheetRect(sheetRect());
        applyLayers();
        if (!area)
            render->setOverlay(true);
        if (wasRunning)
            sim->start();
        qInfo() << "Hoja reubicada en" << mapping.sheetWidth << "x" << mapping.sheetHeight << "px desde"
                << mapping.sheetX << mapping.sheetY;
        if (onSheetChanged)
            onSheetChanged();
    }

    void startCalibration()
    {
        picker->hide();
        calibrationHost->setGeometry(screen->geometry());
        calibrationHost->show();
        calibrationHost->raise();
        calibrationHost->activateWindow();
        calibration->start();
    }

    void finishCalibration(const QRect& rect)
    {
        // La ventana de calibración cubre el monitor: el rectángulo ya es relativo a su
        // esquina, como lo guarda appkit, pero en píxeles de Qt: se pasa a físicos.
        calibrationHost->hide();
        appkit::saveUsableArea(*config, appkit::ScreenId::of(screen), appkit::scaledRect(rect, screen->devicePixelRatio()));
        qInfo() << "Área útil calibrada en" << appkit::ScreenId::of(screen).describe() << "· Qt" << rect << "· físicos" << appkit::scaledRect(rect, screen->devicePixelRatio());
        relayout();
        focusCanvas();
    }

    // F10: el lienzo nativo no cambia de tamaño en caliente, así que se guarda el monitor
    // siguiente y la app se reinicia ahí (lo dibujado se pierde, como al cambiar de monitor).
    void nextScreen()
    {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (screens.size() < 2)
            return;
        QScreen* next = screens[(screens.indexOf(screen) + 1) % screens.size()];
        config->setValue(QStringLiteral("monitor"), appkit::ScreenId::of(next).toVariant(), appkit::Config::Scope::Common);
        qInfo() << "Monitor siguiente:" << appkit::ScreenId::of(next).describe() << "· reiniciando";
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
        QApplication::quit();
    }

    // La mina activa y la goma (con lo calibrado en vivo) pasan a la simulación.
    void applyMedia()
    {
        media.updateUnsaved();
        if (sim) {
            sim->setGrade(media.active); // antes que la mina: el desgaste es de esta dureza
            sim->setLead(media.lead());
            sim->setEraser(media.current.eraser);
        }
        if (refreshPanel)
            refreshPanel();
    }

    // Ctrl+S y los botones Guardar del panel.
    void saveLead()
    {
        const size_t a = size_t(media.active.load());
        media.saved.grades[a] = media.current.grades[a];
        if (media.write())
            qInfo() << "Guardada la" << media.activeName() << "en" << media.path;
        applyMedia();
    }

    void saveEraser()
    {
        media.saved.eraser = media.current.eraser;
        if (media.write())
            qInfo() << "Guardada la goma en" << media.path;
        applyMedia();
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

    // El foco vuelve al lienzo nativo (las teclas y el lápiz siguen ahí).
    // El lienzo, la hoja y el área útil están en píxeles físicos; las ventanas de Qt (panel,
    // selector, menús) se ubican en los de Qt, que con escala de Windows son más grandes.
    QRect toQt(const QRect& physical) const { return appkit::scaledRect(physical, 1.0 / screen->devicePixelRatio()); }

    void focusCanvas()
    {
        shell->activateWindow();
        SetFocus(canvas->hwnd());
    }

    // Un overlay (menú, atajos) centrado sobre la hoja, con el foco del teclado.
    void showOverlay(QWidget* overlay)
    {
        const QRect sheet = toQt({mapping.sheetX, mapping.sheetY, mapping.sheetWidth, mapping.sheetHeight});
        overlay->move(shell->mapToGlobal(sheet.center() - QPoint(overlay->width() / 2, overlay->height() / 2)));
        overlay->show();
        overlay->raise();
        overlay->activateWindow();
        overlay->setFocus();
    }

    // Ctrl+, (HU-78): la lista sale del registro al abrirla, con lo que la app haya sumado.
    void toggleShortcuts()
    {
        if (shortcutsOverlay->isVisible()) {
            shortcutsOverlay->hide();
            qInfo() << "Atajos: cerrados";
            focusCanvas();
            return;
        }
        shortcutsOverlay->setSheet(QApplication::applicationName(), shortcuts.sheet());
        showOverlay(shortcutsOverlay.get());
        qInfo() << "Atajos: abiertos";
    }

    void togglePicker()
    {
        if (picker->isVisible()) {
            picker->hide();
            focusCanvas();
            return;
        }
        picker->setLeads(media.current.grades, media.active, mapping.pixelsPerCellX);
        // A la derecha de la hoja, arriba, en coordenadas de pantalla.
        const QRect sheet = toQt({mapping.sheetX, mapping.sheetY, mapping.sheetWidth, mapping.sheetHeight});
        const QPoint corner = shell->mapToGlobal(QPoint(sheet.right() + 1 - LeadPicker::kWidth - 16, sheet.top() + 16));
        picker->move(corner);
        picker->show();
        picker->raise();
        picker->activateWindow();
        picker->setFocus();
    }

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
        wchar_t text[768];
        const wchar_t* warning =
            area ? L"" : L"\nSin área calibrada en este monitor: calibrala con F9.";
        if (!sim->erasing() && sim->stylus()) {
            swprintf(text, 768, L"punta seca: hunde el papel sin dejar grafito (E vuelve a la mina)   ·   costado %ls (I)",
                     tilt ? L"sí" : L"no");
        } else if (sim->erasing()) {
            const Eraser eraser = sim->eraser();
            const bool unsaved = media.eraserUnsaved;
            swprintf(text, 768, L"goma%ls   ·   fuerza %d (, .)   ·   %.1f mm ([ ])%ls", unsaved ? L"*" : L"",
                     eraser.strength, eraser.diameter / 100.0, unsaved ? L"   ·   Ctrl+S guarda" : L"");
        } else {
            const Lead lead = sim->lead();
            const bool unsaved = media.leadUnsaved;
            swprintf(text, 768,
                     L"mina %hs%ls (F5)   ·   blandura %d (, .)   ·   %.2f mm ([ ])   ·   techo %d %% (- =)   ·   punta %d %% gastada (A afila)   ·   costado %ls (I)%ls",
                     media.activeName(), unsaved ? L"*" : L"", lead.softness, lead.diameter / 100.0,
                     int(std::lround(lead.ceiling * 100.0 / 65535)), sim->wearPercent(), tilt ? L"sí" : L"no",
                     unsaved ? L"   ·   Ctrl+S guarda" : L"");
        }
        std::wstring out = text;
        swprintf(text, 768, L"   ·   textura %d %% (Ctrl+[ ])%ls", texture, warning);
        wchar_t version[128];
        swprintf(version, 128, L"escala de Windows %d %%   ·   hoja %d × %d px\n",
                 int(std::lround(screen->devicePixelRatio() * 100)), mapping.sheetWidth, mapping.sheetHeight);
        return version + out + text;
    }
};

Lienzo::Lienzo(Shell& shell, QScreen* screen, appkit::Config& config, LienzoOptions options)
    : d(std::make_unique<Impl>())
{
    d->options = std::move(options);
    d->shell = &shell;
    d->screen = screen;
    d->config = &config;

    d->media.path = QDir(appkit::familyDataDirectory()).filePath(QStringLiteral("medios.json"));
    d->media.load();
    d->restoreState();

    registerShortcuts();
    const auto dispatch = [impl = d.get()](UINT vk, bool ctrl) { impl->shortcuts.trigger(vk, ctrl); };
    shell.onKey = dispatch;
    d->canvas = std::make_unique<CanvasWindow>(reinterpret_cast<HWND>(shell.winId()), d->queue, dispatch);

    // Área útil: la de la familia de apps (sección común de config.json), la que se calibra
    // con F9.
    d->computeMapping();
    d->image.width = d->canvas->width();
    d->image.height = d->canvas->height();
    d->image.pixels.resize(size_t(d->image.width) * size_t(d->image.height));
    d->createSimulation();
    d->render = std::make_unique<Renderer>(d->canvas->hwnd(), d->image);
    d->render->setTimings(&d->timings);
    d->render->setRotation(d->rotation()); // el giro guardado (HU-13)
    d->render->setSheetRect(d->sheetRect());
    d->render->setExtraInfo([impl = d.get()] { return impl->overlayText(); });
    if (!d->area)
        d->render->setOverlay(true); // el aviso tiene que verse sin apretar nada

    d->calibrationHost = std::make_unique<QWidget>(&shell, Qt::Tool | Qt::FramelessWindowHint);
    d->calibration = new appkit::CalibrationOverlay(d->calibrationHost.get());
    QObject::connect(d->calibration, &appkit::CalibrationOverlay::finished,
                     [impl = d.get()](const QRect& rect) { impl->finishCalibration(rect); });
    QObject::connect(d->calibration, &appkit::CalibrationOverlay::cancelled, [impl = d.get()] {
        impl->calibrationHost->hide();
        impl->focusCanvas();
    });

    d->canvas->setOnRotateGesture(
        [impl = d.get()](int phase, double x, double y) { impl->rotateGesture(phase, x, y); });
    // Rueda táctil (HU-74): giro libre, sin snap; se guarda al levantar el dedo.
    d->canvas->setOnRing([impl = d.get()](double degrees, bool ended) {
        if (degrees != 0) {
            impl->setViewRotation(impl->viewDegrees + degrees, false);
            impl->ringTurned = true;
        }
        if (ended && impl->ringTurned) {
            impl->ringTurned = false;
            impl->config->setValue(QStringLiteral("viewRotation"), impl->viewDegrees);
            qInfo() << "Vista rotada con la rueda" << impl->viewDegrees << "°";
        }
    });

    d->picker = std::make_unique<LeadPicker>(&shell);
    d->picker->onPick = [impl = d.get()](int grade) {
        impl->setActive(grade);
        impl->focusCanvas();
    };
    d->picker->onClose = [impl = d.get()] { impl->focusCanvas(); };
    d->shortcutsOverlay = std::make_unique<appkit::ShortcutsOverlay>(&shell);
    d->shortcutsOverlay->onClose = [impl = d.get()] {
        qInfo() << "Atajos: cerrados";
        impl->focusCanvas();
    };

    // Recarga en caliente de medios.json. Muchos editores guardan reemplazando el archivo y
    // el watcher lo pierde: se vuelve a agregar.
    d->watcher = std::make_unique<QFileSystemWatcher>(QStringList{d->media.path});
    QObject::connect(d->watcher.get(), &QFileSystemWatcher::fileChanged, [impl = d.get()] {
        if (!impl->watcher->files().contains(impl->media.path) && QFile::exists(impl->media.path))
            impl->watcher->addPath(impl->media.path);
        if (impl->media.load()) {
            impl->applyMedia();
            if (impl->picker->isVisible())
                impl->picker->setLeads(impl->media.current.grades, impl->media.active, impl->mapping.pixelsPerCellX);
        }
    });

    qInfo() << "Trazos" << ET_VERSION << "·" << QApplication::applicationName() << "· escala de Windows" << screen->devicePixelRatio() << "· lienzo"
            << d->canvas->width() << "x" << d->canvas->height() << "px físicos";
    qInfo() << "Monitor" << appkit::ScreenId::of(screen).describe() << (d->area ? "· área útil" : "· SIN área útil")
            << (d->area ? *d->area : QRect());
    qInfo() << "Hoja" << d->paper.width() << "x" << d->paper.height() << "celdas en" << d->mapping.sheetWidth << "x"
            << d->mapping.sheetHeight << "px desde" << d->mapping.sheetX << d->mapping.sheetY << "("
            << d->mapping.pixelsPerCellX << "x" << d->mapping.pixelsPerCellY << "px por celda)";
}

Lienzo::~Lienzo()
{
    stop();
}

appkit::Shortcuts& Lienzo::shortcuts()
{
    return d->shortcuts;
}

void Lienzo::setOnSheetChanged(std::function<void()> callback)
{
    d->onSheetChanged = std::move(callback);
}

void Lienzo::setOnStylusButton(std::function<void()> callback)
{
    d->canvas->setOnStylusButton(std::move(callback));
}

void Lienzo::registerShortcuts()
{
    appkit::Shortcuts& keys = d->shortcuts;
    Impl* impl = d.get();
    MediaFile& media = d->media;
    // Grupos de la lista de atajos (Ctrl+, HU-78), en las columnas de la mesa "Atajos"; la
    // app ubica los suyos (Ejercicios) con setColumns.
    keys.setColumns({{kGroupLead}, {kGroupSheet, kGroupView}, {kGroupScreen}});

    keys.setGroup(kGroupLead);
    keys.add(QStringLiteral("Selector de lápices"), {{VK_F5}}, [impl] { impl->togglePicker(); }); // HU-67
    // [ y ] tamaño, pasos de ~15 % (pedido del usuario: como el tamaño del pincel).
    const auto size = [impl, &media](bool up) {
        if (impl->erasing()) // goma: 2 a 8 mm
            impl->adjust(media.current.eraser.diameter, up, 1.15, 200, 800);
        else // mina: 0,30 a 2,00 mm (punta de 48 celdas)
            impl->adjust(media.lead().diameter, up, 1.15, 30, 200);
    };
    keys.add(QStringLiteral("Achicar"), {{VK_OEM_4}}, [size] { size(false); });
    keys.add(QStringLiteral("Agrandar"), {{VK_OEM_6}}, [size] { size(true); });
    keys.joinWithPrevious(QStringLiteral("Tamaño de la mina"));
    // , y . blandura de la mina o fuerza de la goma, pasos de ~25 %.
    const auto softness = [impl, &media](bool up) {
        int& value = impl->erasing() ? media.current.eraser.strength : media.lead().softness;
        impl->adjust(value, up, 1.25, 1, 255);
    };
    keys.add(QStringLiteral("Menos blandura o fuerza"), {{VK_OEM_COMMA}}, [softness] { softness(false); });
    keys.add(QStringLiteral("Más blandura o fuerza"), {{VK_OEM_PERIOD}}, [softness] { softness(true); });
    keys.joinWithPrevious(QStringLiteral("Blandura o fuerza"));
    // - y = techo de tono de la mina, pasos de ~10 % (la goma no tiene).
    const auto ceiling = [impl, &media](bool up) {
        if (!impl->erasing())
            impl->adjust(media.lead().ceiling, up, 1.1, 2000, 65535);
    };
    keys.add(QStringLiteral("Bajar el techo"), {{VK_OEM_MINUS}}, [ceiling] { ceiling(false); });
    keys.add(QStringLiteral("Subir el techo"), {{VK_OEM_PLUS}}, [ceiling] { ceiling(true); });
    keys.joinWithPrevious(QStringLiteral("Techo de tono"));
    keys.add(QStringLiteral("Afilar"), {{'A'}}, [impl] { impl->sharpen(); });                 // HU-62
    keys.add(QStringLiteral("Punta seca o mina"), {{'E'}}, [impl] { impl->toggleStylus(); }); // HU-61
    keys.add(QStringLiteral("Costado sí o no"), {{'I'}}, [impl] { impl->toggleTilt(); });     // HU-73
    keys.add(QStringLiteral("Guardar el lápiz"), {{'S', true}}, [impl] {
        if (impl->erasing())
            impl->saveEraser();
        else
            impl->saveLead();
    });

    keys.setGroup(kGroupSheet);
    keys.add(QStringLiteral("Hoja nueva"), {{'N', true}}, [this] { clear(); });
    if (d->options.undo) {
        keys.add(QStringLiteral("Deshacer"), {{'Z'}}, [impl] { impl->sim->requestUndo(); }); // Z sola (pedido del usuario, 10 de octubre)
        keys.add(QStringLiteral("Rehacer"), {{'Y', true}}, [impl] { impl->sim->requestRedo(); });
    }
    keys.add(QStringLiteral("Menos textura del papel"), {{VK_OEM_4, true}}, [impl] { impl->stepTexture(false); }); // HU-75
    keys.add(QStringLiteral("Más textura del papel"), {{VK_OEM_6, true}}, [impl] { impl->stepTexture(true); });
    keys.joinWithPrevious(QStringLiteral("Textura del papel"));

    keys.setGroup(kGroupView);
    { // los números son de la vista (HU-40), en las dos apps; la dureza se elige con F5
        keys.add(QStringLiteral("Girar la vista a la izquierda"), {{'4'}, {VK_NUMPAD4}}, [impl] {
            impl->setViewRotation(ViewRotation::snapped(impl->viewDegrees) - ViewRotation::kSnapStep);
            qInfo() << "Vista rotada" << impl->viewDegrees << "° (tecla)";
        });
        keys.add(QStringLiteral("Girar la vista a la derecha"), {{'6'}, {VK_NUMPAD6}}, [impl] {
            impl->setViewRotation(ViewRotation::snapped(impl->viewDegrees) + ViewRotation::kSnapStep);
            qInfo() << "Vista rotada" << impl->viewDegrees << "° (tecla)";
        });
        keys.joinWithPrevious(QStringLiteral("Girar de a 15°"));
        keys.add(QStringLiteral("Vista a 0°"), {{'5'}, {VK_NUMPAD5}}, [impl] {
            impl->setViewRotation(0);
            qInfo() << "Vista rotada 0 ° (tecla)";
        });
    }
    keys.note(QStringLiteral("Shift+arrastrar"), QStringLiteral("Girar libre con el lápiz")); // HU-79
    keys.note(QStringLiteral("Rueda"), QStringLiteral("Girar con la rueda de la tableta"));   // HU-74

    keys.setGroup(kGroupScreen);
    keys.add(QStringLiteral("Atajos"), {{VK_OEM_COMMA, true}}, [impl] { impl->toggleShortcuts(); }); // HU-78
    keys.add(QStringLiteral("Latencia y herramienta"), {{VK_F3}}, [impl] { impl->render->toggleOverlay(); });
    keys.add(QStringLiteral("Calibrar el área útil"), {{VK_F9}}, [impl] { impl->startCalibration(); }); // HU-66
    keys.add(QStringLiteral("Monitor siguiente"), {{VK_F10}}, [impl] { impl->nextScreen(); });          // HU-66
    keys.add(QStringLiteral("Imagen de pantalla"), {{VK_F12}}, [impl] { impl->saveScreenImage(); });    // diagnóstico
    keys.note(QStringLiteral("Alt+F4"), QStringLiteral("Salir"));
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
    d->shortcuts.reportConflicts(d->shell);
    const QStringList args = QApplication::arguments();
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
    // --bench [undo] [costado]: trazos sintéticos durante 30 s, para medir sin la tableta.
    if (args.contains(QStringLiteral("--bench"))) {
        const bool withUndo = args.contains(QStringLiteral("undo")) && d->options.undo;
        const bool side = args.contains(QStringLiteral("costado"));
        qInfo() << "Benchmark sintético" << (withUndo ? "con deshacer" : "sin deshacer")
                << (side ? "· 6B de costado" : "");
        d->bench = std::thread([impl = d.get(), withUndo, side] {
            runBench(impl->queue, *impl->sim, impl->mapping, withUndo, side);
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
    d->guides = guides;
    d->sim->setGuides(d->guideLayer());
}

void Lienzo::showOverlay(QWidget* overlay)
{
    d->showOverlay(overlay);
}

int Lienzo::sheetCount() const
{
    return d->order.count;
}

int Lienzo::activeSheet() const
{
    return d->order.active;
}

void Lienzo::addSheet(int at)
{
    d->order.add(at);
    d->sim->requestAddSheet(at);
}

void Lienzo::removeSheet(int index)
{
    if (!d->order.valid(index))
        return;
    const uint64_t id = d->order.ids[size_t(index)];
    if (!d->order.remove(index))
        return;
    d->sim->requestRemoveSheet(index);
    d->render->dropSheet(id);
}

void Lienzo::setUnderlays(const std::vector<Underlay>& underlays)
{
    d->underlays = underlays;
    d->applyLayers();
}

void Lienzo::showSheet(int index)
{
    d->shownSheet = index;
    d->applyLayers();
}

void Lienzo::moveSheet(int from, int to)
{
    if (!d->order.valid(from) || !d->order.valid(to) || from == to)
        return;
    d->order.move(from, to);
    d->sim->requestMoveSheet(from, to);
}

void Lienzo::activateSheet(int index)
{
    if (!d->order.valid(index) || index == d->order.active)
        return;
    d->order.activate(index);
    d->sim->requestActivateSheet(index);
}

void Lienzo::focusCanvas()
{
    d->focusCanvas();
}

void Lienzo::addPanelTabs(appkit::SidePanel& panel)
{
    Impl* impl = d.get();
    auto* tools = new ToolPage;
    tools->onLeadChanged = [impl](const Lead& lead) {
        impl->media.lead() = lead;
        impl->applyMedia();
    };
    tools->onEraserChanged = [impl](const Eraser& eraser) {
        impl->media.current.eraser = eraser;
        impl->applyMedia();
    };
    tools->onSaveLead = [impl] { impl->saveLead(); };
    tools->onSaveEraser = [impl] { impl->saveEraser(); };
    panel.addTab(tools, QStringLiteral("Lápiz"));

    auto* screenPage = new QWidget;
    auto* layout = new QVBoxLayout(screenPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto* monitor = new QLabel(screenPage);
    monitor->setWordWrap(true);
    auto* calibrate = new QPushButton(QStringLiteral("Calibrar el área útil (F9)"), screenPage);
    auto* next = new QPushButton(QStringLiteral("Monitor siguiente (F10)"), screenPage);
    next->setEnabled(QGuiApplication::screens().size() > 1);
    auto* nextHint = new QLabel(QStringLiteral("La app se reinicia en el otro monitor; lo dibujado se pierde."), screenPage);
    nextHint->setObjectName(QStringLiteral("secundario"));
    nextHint->setWordWrap(true);
    layout->addWidget(monitor);
    layout->addWidget(calibrate);
    layout->addSpacing(8);
    layout->addWidget(next);
    layout->addWidget(nextHint);
    QObject::connect(calibrate, &QPushButton::clicked, [impl, &panel] {
        panel.hide();
        impl->startCalibration();
    });
    QObject::connect(next, &QPushButton::clicked, [impl] { impl->nextScreen(); });
    panel.addTab(screenPage, QStringLiteral("Pantalla"));

    d->refreshPanel = [impl, tools, monitor] {
        tools->setTools(QString::fromLatin1(impl->media.activeName()), impl->media.lead(), impl->media.leadUnsaved,
                        impl->media.current.eraser, impl->media.eraserUnsaved);
        const QRect a = impl->area.value_or(QRect());
        monitor->setText(QStringLiteral("Monitor: %1\nÁrea útil: %2")
                             .arg(appkit::ScreenId::of(impl->screen).describe(),
                                  impl->area ? QStringLiteral("%1 × %2 px desde (%3, %4)")
                                                   .arg(a.width())
                                                   .arg(a.height())
                                                   .arg(a.x())
                                                   .arg(a.y())
                                             : QStringLiteral("sin calibrar")));
    };
    d->refreshPanel();
}

void Lienzo::showSidePanel(appkit::SidePanel& panel)
{
    // Pegado al borde derecho del área útil (lo que alcanza el lápiz), arriba.
    const QRect a = d->toQt(d->area.value_or(sheetRect()));
    const int margin = 16;
    panel.resize(appkit::SidePanel::kWidth, std::min(680, a.height() - 2 * margin));
    panel.move(d->shell->mapToGlobal(QPoint(a.right() + 1 - panel.width() - margin, a.top() + margin)));
    if (d->refreshPanel)
        d->refreshPanel();
    panel.show();
    panel.raise();
    panel.activateWindow();
    panel.setFocus();
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
