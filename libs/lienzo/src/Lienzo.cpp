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
#include <QMetaObject>
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

void Shell::keyReleaseEvent(QKeyEvent* event)
{
    if (onKeyUp && !event->isAutoRepeat())
        onKeyUp(UINT(event->nativeVirtualKey()));
}

void Shell::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    if (onKeyUp)
        onKeyUp(0);
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
    explicit Impl(LienzoOptions o)
        : options(std::move(o))
        , paper({.widthMm = kTabletWidthMm,
                 .heightMm = kTabletHeightMm,
                 .dpi = profileDpi(options.profile),
                 .grain = options.paperGrain})
    {
    }
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
    drymedia::Paper paper; // con la resolución del perfil (HU-112)
    // Pila de hojas (HU-82): la cambia solo la simulación; acá, junto al papel, sobrevive a
    // recrearla (F9). `order` es lo mismo visto desde la interfaz (sheetCount, activeSheet).
    SheetStack sheets{Simulation::kUndoLimit};
    SheetOrder order;
    // HU-87: el deshacer cambia la pila en la simulación, así que `order` se corrige con lo que
    // ella publica cuando atendió todo lo que se le pidió (pedidos de pila, deshacer, rehacer).
    uint64_t sentRequests = 0;
    template <typename F>
    void post(F request)
    {
        ++sentRequests;
        request();
    }
    SheetOrder& syncOrder()
    {
        const Simulation::SheetState state = sim->sheetState();
        if (state.applied == sentRequests)
            order = state.order;
        return order;
    }
    void requestUndo()
    {
        beforeSheetChange();
        post([this] { sim->requestUndo(); });
    }
    void requestRedo()
    {
        beforeSheetChange();
        post([this] { sim->requestRedo(); });
    }
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
                                                paper.spec().heightMm, paper.width(), paper.height(), paper.cellsPerMm())
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
        order = sheets.order(); // la simulación nueva todavía no atendió nada
        sentRequests = 0;
        sim->setUndo(options.undo);
        sim->setProfile(options.profile);
        sim->setPaperColor(options.paperColor);
        // Deshacer cambia la pila sin que la interfaz lo pida: la mesa de luz se recalcula al
        // aviso (encolado: si el lienzo ya no está, relay tampoco y el aviso se descarta).
        sim->setOnSheetState([this] { QMetaObject::invokeMethod(&relay, [this] { applyLayers(); }, Qt::QueuedConnection); });
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

    // Mesa de luz (HU-91) y la hoja que se muestra (HU-83, flip HU-90), pasadas a ids para el
    // render con la pila como está.
    LightTable lightTable;
    int shownSheet = -1;
    QObject relay; // la simulación avisa que cambió la pila; acá se atiende en este hilo
    void applyLayers()
    {
        if (!render)
            return;
        syncOrder();
        std::vector<SheetLayer> below;
        if (!flipping()) // mientras se flipea, la mesa de luz no se ve (HU-90)
            below = lightTableLayers(order, lightTable);
        uint64_t shown = 0;
        bool untilActive = false;
        if (order.valid(shownSheet) && (flipping() || shownSheet != order.active))
            shown = order.ids[size_t(shownSheet)];
        else if (releasedTo) {
            shown = releasedTo;
            untilActive = true;
        }
        render->setLayers(shown, std::move(below), untilActive);
    }

    // Flip (HU-90): mientras haya una tecla de flip apretada se muestra shownSheet; al soltar
    // la última, esa hoja se activa y el render la sigue mostrando desde su textura hasta que
    // la imagen de pantalla la tenga entera (releasedTo).
    std::vector<UINT> flipKeys; // las teclas de flip que siguen apretadas
    UINT pressedKey = 0;        // la tecla cuyo atajo se está atendiendo
    uint64_t releasedTo = 0;
    bool flipping() const { return !flipKeys.empty(); }
    void flip(int step)
    {
        SheetOrder& o = syncOrder();
        const bool starting = !flipping();
        if (starting) {
            releasedTo = 0;
            shownSheet = o.active;
            canvas->setPenBlocked(true);
        }
        if (pressedKey && std::find(flipKeys.begin(), flipKeys.end(), pressedKey) == flipKeys.end())
            flipKeys.push_back(pressedKey);
        const int next = shownSheet + step;
        const bool moved = o.valid(next); // en los extremos se queda
        if (moved) {
            shownSheet = next;
            qInfo().noquote() << QStringLiteral("Flip: hoja %1 de %2").arg(next + 1).arg(o.count);
        }
        if (starting || moved)
            applyLayers();
        if (!flipping()) // sin tecla (una llamada suelta): se suelta enseguida
            endFlip();
    }
    void keyUp(UINT vk)
    {
        if (!flipping())
            return;
        if (vk == 0)
            flipKeys.clear();
        else
            flipKeys.erase(std::remove(flipKeys.begin(), flipKeys.end(), vk), flipKeys.end());
        if (!flipping())
            endFlip();
    }
    void endFlip()
    {
        flipKeys.clear();
        canvas->setPenBlocked(false);
        SheetOrder& o = syncOrder();
        const int target = shownSheet;
        shownSheet = -1;
        if (o.valid(target) && target != o.active) {
            o.activate(target);
            releasedTo = o.ids[size_t(target)];
            post([this, target] { sim->requestActivateSheet(target); });
            qInfo().noquote() << QStringLiteral("Flip: activa la hoja %1 de %2").arg(target + 1).arg(o.count);
        }
        applyLayers();
    }
    // Antes de otro cambio de la pila (o deshacer): el flip termina y la hoja soltada deja de
    // esperarse (si la imagen pasa a ser de otra hoja, el render la esperaría para siempre).
    void beforeSheetChange()
    {
        if (flipping())
            endFlip();
        if (releasedTo) {
            releasedTo = 0;
            applyLayers();
        }
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
        media.active = startingGrade(options.fixedGrade, config->value(QStringLiteral("lead")).toString());
        viewDegrees = ViewRotation::normalized(config->value(QStringLiteral("viewRotation"), 0.0).toDouble());
        tilt = config->value(QStringLiteral("tilt"), options.tiltByDefault).toBool();
        texture = std::clamp(config->value(QStringLiteral("paperTexture"), 20, appkit::Config::Scope::Common).toInt(), 0, 100);
        if (!options.paperTexture)
            texture = 0; // papel liso (HU-97): no se lee ni se guarda la de la familia
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
        qInfo() << "Selector de lápices: abierto";
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
            wchar_t wear[64] = L""; // sin desgaste en el perfil de animación (HU-112)
            if (options.profile == MediumProfile::Illustration)
                swprintf(wear, 64, L"   ·   punta %d %% gastada (A afila)", sim->wearPercent());
            if (options.fixedGrade >= 0) // un solo grafito (HU-98): no se calibra acá
                swprintf(text, 768, L"grafito %hs   ·   %.2f mm   ·   costado %ls (I)", media.activeName(),
                         lead.diameter / 100.0, tilt ? L"sí" : L"no");
            else
                swprintf(text, 768,
                         L"mina %hs%ls (F5)   ·   blandura %d (, .)   ·   %.2f mm ([ ])   ·   techo %d %% (- =)%ls   ·   costado %ls (I)%ls",
                         media.activeName(), unsaved ? L"*" : L"", lead.softness, lead.diameter / 100.0,
                         int(std::lround(lead.ceiling * 100.0 / 65535)), wear, tilt ? L"sí" : L"no",
                         unsaved ? L"   ·   Ctrl+S guarda" : L"");
        }
        std::wstring out = text;
        if (options.sheets) { // HU-87: qué hoja es (lo que ya atendió la simulación)
            const SheetOrder o = sim->sheetState().order;
            swprintf(text, 768, L"   ·   hoja %d de %d", o.active + 1, o.count);
            out += text;
        }
        if (options.paperTexture)
            swprintf(text, 768, L"   ·   textura %d %% (Ctrl+[ ])%ls", texture, warning);
        else
            swprintf(text, 768, L"%ls", warning);
        wchar_t version[128];
        swprintf(version, 128, L"escala de Windows %d %%   ·   hoja %d × %d px\n",
                 int(std::lround(screen->devicePixelRatio() * 100)), mapping.sheetWidth, mapping.sheetHeight);
        return version + out + text;
    }
};

Lienzo::Lienzo(Shell& shell, QScreen* screen, appkit::Config& config, LienzoOptions options)
    : d(std::make_unique<Impl>(std::move(options)))
{
    d->shell = &shell;
    d->screen = screen;
    d->config = &config;

    d->media.path = QDir(appkit::familyDataDirectory()).filePath(QStringLiteral("medios.json"));
    d->media.load();
    d->restoreState();

    registerShortcuts();
    const auto dispatch = [impl = d.get()](UINT vk, bool ctrl) {
        impl->pressedKey = vk; // el flip (HU-90) anota qué tecla soltar
        impl->shortcuts.trigger(vk, ctrl);
        impl->pressedKey = 0;
    };
    const auto keyUp = [impl = d.get()](UINT vk) { impl->keyUp(vk); };
    shell.onKey = dispatch;
    shell.onKeyUp = keyUp;
    d->canvas = std::make_unique<CanvasWindow>(reinterpret_cast<HWND>(shell.winId()), d->queue, dispatch);
    d->canvas->setOnKeyUp(keyUp);

    // Área útil: la de la familia de apps (sección común de config.json), la que se calibra
    // con F9.
    d->computeMapping();
    d->image.width = d->canvas->width();
    d->image.height = d->canvas->height();
    d->image.pixels.resize(size_t(d->image.width) * size_t(d->image.height));
    d->createSimulation();
    d->render = std::make_unique<Renderer>(d->canvas->hwnd(), d->image);
    d->render->setTimings(&d->timings);
    d->render->setPaperColor(d->options.paperColor);
    d->render->setRotation(d->rotation()); // el giro guardado (HU-13)
    d->render->setSheetRect(d->sheetRect());
    d->render->setExtraInfo([impl = d.get()] { return impl->overlayText(); });
    d->render->setOnFlip([](double ms) { // RNF-01 (HU-90): lo verifica scripts/smoke.ps1
        qInfo().noquote() << QStringLiteral("Flip presentado en %1 ms").arg(ms, 0, 'f', 1);
    });
    d->render->setOnLightTable([](double ms) { // RNF-03 (HU-91), ídem
        qInfo().noquote() << QStringLiteral("Mesa de luz presentada en %1 ms").arg(ms, 0, 'f', 1);
    });
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
    d->picker->setPaper(d->paper.cellsPerMm(), d->options.profile);
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
    const bool calibrable = d->options.fixedGrade < 0; // con un solo grafito (HU-98), nada de esto
    if (calibrable)
        keys.add(QStringLiteral("Selector de lápices"), {{VK_F5}}, [impl] { impl->togglePicker(); }); // HU-67
    // [ y ] tamaño, pasos de ~15 % (pedido del usuario: como el tamaño del pincel).
    const auto size = [impl, &media](bool up) {
        if (impl->erasing()) // goma: 2 a 8 mm
            impl->adjust(media.current.eraser.diameter, up, 1.15, 200, 800);
        else // mina: 0,30 a 2,00 mm (punta de 48 celdas)
            impl->adjust(media.lead().diameter, up, 1.15, 30, 200);
    };
    if (calibrable) {
        keys.add(QStringLiteral("Achicar"), {{VK_OEM_4}}, [size] { size(false); });
        keys.add(QStringLiteral("Agrandar"), {{VK_OEM_6}}, [size] { size(true); });
        keys.joinWithPrevious(QStringLiteral("Tamaño de la mina"));
    }
    // , y . blandura de la mina o fuerza de la goma, pasos de ~25 %.
    const auto softness = [impl, &media](bool up) {
        int& value = impl->erasing() ? media.current.eraser.strength : media.lead().softness;
        impl->adjust(value, up, 1.25, 1, 255);
    };
    if (calibrable) {
        keys.add(QStringLiteral("Menos blandura o fuerza"), {{VK_OEM_COMMA}}, [softness] { softness(false); });
        keys.add(QStringLiteral("Más blandura o fuerza"), {{VK_OEM_PERIOD}}, [softness] { softness(true); });
        keys.joinWithPrevious(QStringLiteral("Blandura o fuerza"));
    }
    // - y = techo de tono de la mina, pasos de ~10 % (la goma no tiene).
    const auto ceiling = [impl, &media](bool up) {
        if (!impl->erasing())
            impl->adjust(media.lead().ceiling, up, 1.1, 2000, 65535);
    };
    if (calibrable) {
        keys.add(QStringLiteral("Bajar el techo"), {{VK_OEM_MINUS}}, [ceiling] { ceiling(false); });
        keys.add(QStringLiteral("Subir el techo"), {{VK_OEM_PLUS}}, [ceiling] { ceiling(true); });
        keys.joinWithPrevious(QStringLiteral("Techo de tono"));
    }
    if (d->options.profile == MediumProfile::Illustration) { // sin desgaste ni punta seca en animación (HU-112)
        keys.add(QStringLiteral("Afilar"), {{'A'}}, [impl] { impl->sharpen(); });                 // HU-62
        keys.add(QStringLiteral("Punta seca o mina"), {{'E'}}, [impl] { impl->toggleStylus(); }); // HU-61
    }
    keys.add(QStringLiteral("Costado sí o no"), {{'I'}}, [impl] { impl->toggleTilt(); });     // HU-73
    if (calibrable)
        keys.add(QStringLiteral("Guardar el lápiz"), {{'S', true}}, [impl] {
            if (impl->erasing())
                impl->saveEraser();
            else
                impl->saveLead();
        });

    keys.setGroup(kGroupSheet);
    if (d->options.sheets) // con la pila, limpiar la hoja se deshace (HU-87)
        keys.add(QStringLiteral("Limpiar la hoja"), {{'N', true}}, [impl] {
            impl->beforeSheetChange();
            impl->post([impl] { impl->sim->requestClearSheet(); });
        });
    else
        keys.add(QStringLiteral("Hoja nueva"), {{'N', true}}, [this] { clear(); });
    if (d->options.undo) {
        keys.add(QStringLiteral("Deshacer"), {{'Z'}}, [impl] { impl->requestUndo(); }); // Z sola (pedido del usuario, 10 de octubre)
        keys.add(QStringLiteral("Rehacer"), {{'Y', true}}, [impl] { impl->requestRedo(); });
    }
    if (d->options.paperTexture) { // el papel de animación es liso, sin atajo (HU-97)
        keys.add(QStringLiteral("Menos textura del papel"), {{VK_OEM_4, true}}, [impl] { impl->stepTexture(false); }); // HU-75
        keys.add(QStringLiteral("Más textura del papel"), {{VK_OEM_6, true}}, [impl] { impl->stepTexture(true); });
        keys.joinWithPrevious(QStringLiteral("Textura del papel"));
    }

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
    return d->syncOrder().count;
}

int Lienzo::activeSheet() const
{
    return d->syncOrder().active;
}

bool Lienzo::activeSheetBlank() const
{
    const Simulation::SheetState state = d->sim->sheetState();
    return state.applied == d->sentRequests && state.activeTiles == 0; // con pedidos en camino, no se sabe
}

void Lienzo::addSheet(int at)
{
    d->beforeSheetChange();
    d->syncOrder().add(at);
    d->post([&] { d->sim->requestAddSheet(at); });
    d->applyLayers();
}

void Lienzo::insertSheet()
{
    d->beforeSheetChange();
    SheetOrder& order = d->syncOrder();
    order.add(order.active + 1);
    order.activate(order.active + 1);
    d->post([&] { d->sim->requestInsertSheet(); });
    d->applyLayers();
}

void Lienzo::duplicateSheet()
{
    d->beforeSheetChange();
    SheetOrder& order = d->syncOrder();
    order.add(order.active + 1);
    order.activate(order.active + 1);
    d->post([&] { d->sim->requestDuplicateSheet(); });
    d->applyLayers();
}

void Lienzo::removeSheet(int index)
{
    d->beforeSheetChange();
    SheetOrder& order = d->syncOrder();
    if (!order.valid(index) || !order.remove(index))
        return;
    d->post([&] { d->sim->requestRemoveSheet(index); });
    d->applyLayers();
}

void Lienzo::setLightTable(const LightTable& table)
{
    d->lightTable = table;
    d->applyLayers();
}

void Lienzo::showSheet(int index)
{
    d->beforeSheetChange();
    d->shownSheet = index;
    d->applyLayers();
}

void Lienzo::flip(int step)
{
    d->flip(step);
}

bool Lienzo::flipping() const
{
    return d->flipping();
}

void Lienzo::moveSheet(int from, int to)
{
    d->beforeSheetChange();
    SheetOrder& order = d->syncOrder();
    if (!order.valid(from) || !order.valid(to) || from == to)
        return;
    order.move(from, to);
    d->post([&] { d->sim->requestMoveSheet(from, to); });
    d->applyLayers();
}

void Lienzo::activateSheet(int index)
{
    d->beforeSheetChange();
    SheetOrder& order = d->syncOrder();
    if (!order.valid(index) || index == order.active)
        return;
    order.activate(index);
    d->post([&] { d->sim->requestActivateSheet(index); });
    d->applyLayers();
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
