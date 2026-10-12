#include "lienzo/Lienzo.h"

#include "lienzo/AudioStrip.h"
#include "lienzo/Bench.h"
#include "lienzo/CanvasWindow.h"
#include "lienzo/DisplayImage.h"
#include "lienzo/Export.h"
#include "lienzo/LeadPicker.h"
#include "lienzo/Media.h"
#include "lienzo/Renderer.h"
#include "lienzo/SampleQueue.h"
#include "lienzo/SheetImages.h"
#include "lienzo/FieldGuide.h"
#include "lienzo/SheetPanel.h"
#include "lienzo/SheetStack.h"
#include "lienzo/Simulation.h"
#include "lienzo/Take.h"
#include "lienzo/TakeSaver.h"
#include "lienzo/Timing.h"
#include "lienzo/ToolPage.h"

#include <audio/Audio.h>
#include <audio/Player.h>

#include <cstring>
#include <appkit/CalibrationOverlay.h>
#include <appkit/Config.h>
#include <appkit/Paths.h>
#include <appkit/ScreenChoice.h>
#include <appkit/Shortcuts.h>
#include <appkit/ShortcutsOverlay.h>
#include <appkit/SidePanel.h>
#include <appkit/Theme.h>
#include <appkit/UsableArea.h>
#include <drymedia/Paper.h>

#include <QApplication>
#include <QMetaObject>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHash>
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
#include <QTimer>

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
    std::function<void()> onStylusButton;
    std::unique_ptr<PenMenu> penMenu; // botón lateral (HU-116, Mesa de animación)
    std::unique_ptr<QWidget> calibrationHost; // F9 (HU-66): ventana propia que cubre el monitor
    appkit::CalibrationOverlay* calibration = nullptr;
    std::unique_ptr<LeadPicker> picker; // F5 (HU-67)
    std::unique_ptr<PencilPicker> pencilPicker; // F5 con lápices de color (HU-100, Mesa de animación)
    std::unique_ptr<appkit::ShortcutsOverlay> shortcutsOverlay; // Ctrl+, (HU-78)
    std::optional<QRect> area;
    // La hoja ocupa todo el mapeo de la tableta (HU-69, pedido del usuario: dibuja sobre una
    // A3 que cubre la superficie), así que coincide con el área útil calibrada.
    drymedia::Paper paper; // con la resolución del perfil (HU-112)
    // Pila de hojas (HU-82): la cambia solo la simulación; acá, junto al papel, sobrevive a
    // recrearla (F9). `order` es lo mismo visto desde la interfaz (sheetCount, activeSheet).
    SheetStack sheets{Simulation::kUndoLimit};
    SheetImages images; // las de las hojas que no son la activa, para la toma (HU-85)
    std::wstring takeWarning; // hojas que la toma abrió en blanco (F3)
    std::unique_ptr<TakeSaver> saver; // guardado automático (HU-86); antes que sim, que lo usa
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
        sim.reset(); // primero la vieja: su destructor suelta el aviso onLeave de la pila
        sim = std::make_unique<Simulation>(paper, sheets, queue, image, mapping);
        order = sheets.order(); // la simulación nueva todavía no atendió nada
        sentRequests = 0;
        sim->setUndo(options.undo);
        sim->setProfile(options.profile);
        sim->setPaperColor(options.paperColor);
        sim->setImages(&images);
        if (saver)
            sim->setAutosave(saver.get());
        // Deshacer cambia la pila sin que la interfaz lo pida: la mesa de luz se recalcula al
        // aviso (encolado: si el lienzo ya no está, relay tampoco y el aviso se descarta).
        sim->setOnSheetState([this] { QMetaObject::invokeMethod(&relay, [this] { applyLayers(); }, Qt::QueuedConnection); });
        sim->setTimings(&timings);
        if (recording)
            sim->setRecording(recording);
        applyMedia();
        sim->setColorPencil(colorPencil);
        sim->setTilt(tilt);
        sim->setTexture(texture);
        sim->setGuides(guideLayer());
        sim->setRoles(roles);
        sim->setFixedSheet(fixedSheet);
        sim->setCircle(circle);
        sim->setAudio(audioFile, audioMarks);
        sim->setRotation(rotation());
        if (render)
            render->setRotation(rotation());
        sim->renderAll();
    }

    // La imagen de una hoja que no es la activa, redibujada desde su depósito (HU-85: una toma
    // guardada sin imagen, o con la hoja de otro tamaño en pantalla). Con la simulación detenida.
    std::vector<uint32_t> renderSheet(int index)
    {
        SheetMapping m = mapping; // la región de la hoja sola, con su esquina en (0, 0)
        m.originX -= m.sheetX;
        m.originY -= m.sheetY;
        m.clientWidth = m.sheetWidth;
        m.clientHeight = m.sheetHeight;
        m.sheetX = m.sheetY = 0;
        std::vector<uint32_t> pixels(size_t(m.sheetWidth) * size_t(m.sheetHeight));
        sheets.withSheet(paper, index, [&] {
            const int threads = int(std::max(1u, std::min(8u, std::thread::hardware_concurrency())));
            std::vector<std::thread> pool;
            for (int t = 0; t < threads; ++t) {
                const int y0 = m.sheetHeight * t / threads, y1 = m.sheetHeight * (t + 1) / threads;
                pool.emplace_back([&, y0, y1] {
                    renderTone(paper, m, 0, y0, m.sheetWidth, y1, pixels.data(), nullptr, nullptr, options.paperColor);
                });
            }
            for (std::thread& th : pool)
                th.join();
        });
        return pixels;
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
        const int fixed = fixedIndex(order, fixedSheet);
        const QColor gray = appkit::theme::kHojaFija;
        const uint32_t fixedColor = bgra(gray.red(), gray.green(), gray.blue());
        if (showingFixed()) { // HU-89: dibujando en la fija, la hoja de la que se vino de referencia
            if (const int back = fixedReturnIndex(); back >= 0)
                below.push_back({order.ids[size_t(back)], fixedColor, appkit::theme::kHojaFijaOpacidad});
        } else {
            if (!flipping()) // mientras se flipea, la mesa de luz no se ve (HU-90)
                below = lightTableLayers(order, lightTable, roles, fixedSheet, circle);
            if (fixed >= 0) // HU-89: la fija, debajo de todas, siempre
                below.insert(below.begin(), {fixedSheet, fixedColor, appkit::theme::kHojaFijaOpacidad});
        }
        uint64_t shown = 0;
        bool untilActive = false;
        if (order.valid(shownSheet) && (flipping() || shownSheet != order.active))
            shown = order.ids[size_t(shownSheet)];
        else if (releasedTo) {
            shown = releasedTo;
            untilActive = true;
        }
        applyMark();
        applyChart();
        if (testGuides)
            addTestGuides();
        render->setLayers(shown, std::move(below), untilActive);
        refreshSheetPanel();
    }

    // Rol y número (HU-104): la marca de la hoja que se ve (al flipear, la de delante), en la
    // esquina superior derecha; se vuelve a pintar solo si cambió el número o el rol.
    SheetRoles roles;
    int markNumber = 0;
    SheetRole markRole = SheetRole::None;
    bool markShown = false; // el número 0 ahora es la fija (HU-89): ya no sirve de "sin marca"
    void applyMark()
    {
        if (!options.sheets)
            return;
        const int index = flipping() && order.valid(shownSheet) ? shownSheet : order.active;
        const int number = lienzo::sheetNumber(order, fixedSheet, index); // 0: la fija, "F" (HU-89)
        const SheetRole role = roleOf(roles, order.ids[size_t(index)]);
        if (number == markNumber && role == markRole && markShown)
            return;
        markNumber = number;
        markRole = role;
        markShown = true;
        const double scale = screen->devicePixelRatio();
        const int size = int(std::lround(64 * scale)), margin = int(std::lround(20 * scale));
        const QImage mark = paintSheetMark(number, role, size);
        std::vector<uint32_t> pixels(size_t(size) * size_t(size));
        for (int y = 0; y < size; ++y)
            std::memcpy(pixels.data() + size_t(y) * size_t(size), mark.constScanLine(y), size_t(size) * 4);
        const QColor ink = appkit::theme::kMarcaHoja;
        render->setMark(mapping.sheetWidth - margin - size, margin, size, std::move(pixels),
                        bgra(ink.red(), ink.green(), ink.blue()));
    }

    // Carta de timing (HU-105): el rectángulo de la esquina superior izquierda, solo si la hoja
    // que se ve (al flipear, la de delante) es key. Medidas de la mesa "Carta de timing".
    bool timingChart = false;
    void applyChart()
    {
        if (!options.sheets)
            return;
        const int index = flipping() && order.valid(shownSheet) ? shownSheet : order.active;
        const bool visible = timingChart && order.valid(index) && roleOf(roles, order.ids[size_t(index)]) == SheetRole::Key;
        const double k = mapping.sheetWidth / paper.spec().widthMm;
        const auto mm = [k](double v) { return LONG(std::lround(v * k)); };
        const QColor ink = appkit::theme::kMarcaHoja;
        render->setChart(visible, RECT{mm(kChartMarginMm), mm(kChartMarginMm), mm(kChartMarginMm + kChartWidthMm),
                                       mm(kChartMarginMm + kChartHeightMm)},
                         float(std::max(1.0, screen->devicePixelRatio())), bgra(ink.red(), ink.green(), ink.blue()),
                         appkit::theme::kGuiaCampoOpacidad);
    }
    static constexpr double kChartWidthMm = 35, kChartHeightMm = 22.5, kChartMarginMm = 6;

    // Audio de la toma (HU-106): el archivo (en la carpeta de la toma), lo leído y la franja.
    // Se lee en un hilo aparte (HU-107: la toma abre sin esperarlo); audioGeneration descarta
    // lo que llega de un audio que ya no es el de la toma.
    QString audioFile, audioProblem;
    std::vector<double> audioMarks; // HU-108, en segundos, en orden
    std::shared_ptr<const audio::Clip> clip;
    bool audioLoading = false;
    uint64_t audioGeneration = 0;
    std::thread audioLoader;
    double audioPosition = 0; // el cursor, en segundos
    bool stripVisible = false, stripSent = false;
    AudioStripInfo stripShown;
    QRect stripRect;
    void loadAudio(const QString& dir)
    {
        stopAudio();
        clip.reset();
        audioProblem.clear();
        audioPosition = 0;
        ++audioGeneration;
        if (audioLoader.joinable())
            audioLoader.join();
        audioLoading = !audioFile.isEmpty();
        if (!audioLoading)
            return;
        audioLoader = std::thread([this, generation = audioGeneration, name = audioFile,
                                   path = QDir(dir).filePath(audioFile).toStdWString()] {
            auto read = std::make_shared<audio::Clip>();
            std::string error;
            const Stopwatch time;
            const bool ok = audio::decode(path, *read, error);
            const double ms = time.ms();
            QMetaObject::invokeMethod(
                &relay,
                [this, generation, name, read, ok, error, ms] {
                    if (generation != audioGeneration)
                        return;
                    audioLoading = false;
                    if (!ok) {
                        audioProblem = QString::fromStdString(error);
                        qWarning().noquote() << QStringLiteral("Audio: no se pudo leer %1: %2").arg(name, audioProblem);
                    } else {
                        clip = read;
                        qInfo().noquote() << QStringLiteral("Audio: %1, %2 s, %3 Hz, %4 canales, leído en %5 ms")
                                                 .arg(name)
                                                 .arg(read->seconds(), 0, 'f', 1)
                                                 .arg(read->sampleRate)
                                                 .arg(read->channels)
                                                 .arg(ms, 0, 'f', 0);
                    }
                    refreshStrip();
                },
                Qt::QueuedConnection);
        });
    }

    // Play y pausa (HU-107): el reproductor tiene su hilo; acá un reloj mueve el cursor con la
    // posición que se escucha.
    audio::Player player;
    QTimer playTimer;
    void stopAudio()
    {
        player.pause();
        playTimer.stop();
    }
    void togglePlay()
    {
        if (!clip) {
            qInfo() << (audioLoading ? "Audio: todavía se está leyendo" : "Audio: no hay audio (A elige uno)");
            return;
        }
        if (player.playing()) {
            stopAudio();
            audioPosition = double(player.position()) / clip->sampleRate;
            qInfo().noquote() << QStringLiteral("Audio: pausa en %1").arg(audioTime(audioPosition));
            refreshStrip();
            return;
        }
        size_t from = size_t(audioPosition * clip->sampleRate);
        if (from + 1 >= clip->frames())
            from = 0; // terminó: otra vez desde el principio
        startAudio(from);
    }
    void startAudio(size_t from)
    {
        std::string error;
        if (!player.play(clip, from, error)) {
            qWarning().noquote() << QStringLiteral("Audio: no se pudo reproducir: %1").arg(QString::fromStdString(error));
            return;
        }
        audioPosition = double(from) / clip->sampleRate;
        qInfo().noquote() << QStringLiteral("Audio: play desde %1").arg(audioTime(audioPosition));
        playTimer.start();
        refreshStrip();
    }
    void rewindAudio()
    {
        const bool wasPlaying = player.playing();
        stopAudio();
        audioPosition = 0;
        qInfo() << "Audio: al principio";
        if (wasPlaying && clip)
            startAudio(0);
        refreshStrip();
    }
    // Marcas (HU-108): M en el cursor, Ctrl+M borra la más cercana; con el lápiz, un toque en la
    // onda pone una y cerca de una la agarra y la mueve. El mouse arrastra el cursor.
    int draggedMark = -1;
    bool seekWasPlaying = false;
    void saveMarks()
    {
        sim->setAudio(audioFile, audioMarks);
        refreshStrip();
    }
    void addMarkHere(double seconds)
    {
        const int index = addMark(audioMarks, seconds);
        qInfo().noquote() << QStringLiteral("Audio: marca %1 en %2").arg(index + 1).arg(audioTime(seconds));
        saveMarks();
    }
    void markAtCursor()
    {
        if (!clip) {
            qInfo() << "Audio: no hay audio (A elige uno)";
            return;
        }
        addMarkHere(player.playing() ? double(player.position()) / clip->sampleRate : audioPosition);
    }
    void removeNearestMark()
    {
        const double at = clip && player.playing() ? double(player.position()) / clip->sampleRate : audioPosition;
        const int index = nearestMark(audioMarks, at);
        if (index < 0) {
            qInfo() << "Audio: no hay marcas";
            return;
        }
        qInfo().noquote() << QStringLiteral("Audio: marca %1 borrada (%2)").arg(index + 1).arg(audioTime(audioMarks[size_t(index)]));
        audioMarks.erase(audioMarks.begin() + index);
        saveMarks();
    }
    void stripInput(int phase, double x, double y, bool pen)
    {
        (void)y;
        if (!clip)
            return;
        const double scale = screen->devicePixelRatio();
        const QRect wave = audioWaveRect(stripRect.size(), scale).translated(stripRect.topLeft());
        const double seconds = timeAt(x, wave, clip->seconds());
        if (pen) {
            if (phase == 0) {
                draggedMark = markAt(audioMarks, x, wave, clip->seconds(), 8 * scale);
                if (draggedMark < 0) { // un toque fuera de las marcas: una nueva
                    addMarkHere(seconds);
                    draggedMark = -2; // no se arrastra la recién puesta
                }
            } else if (draggedMark >= 0) {
                draggedMark = moveMark(audioMarks, draggedMark, seconds);
                if (phase == 2)
                    qInfo().noquote() << QStringLiteral("Audio: marca %1 movida a %2").arg(draggedMark + 1).arg(audioTime(seconds));
                saveMarks();
            }
            if (phase == 2)
                draggedMark = -1;
            return;
        }
        // El mouse: el cursor va adonde está; si sonaba, sigue desde ahí al soltar.
        if (phase == 0) {
            seekWasPlaying = player.playing();
            stopAudio();
        }
        audioPosition = seconds;
        refreshStrip();
        if (phase == 2) {
            qInfo().noquote() << QStringLiteral("Audio: cursor en %1").arg(audioTime(seconds));
            if (seekWasPlaying)
                startAudio(size_t(seconds * clip->sampleRate));
        }
    }

    void audioTick()
    {
        if (!clip)
            return;
        audioPosition = double(player.position()) / clip->sampleRate;
        if (!player.playing()) {
            playTimer.stop();
            audioPosition = clip->seconds();
            qInfo() << "Audio: terminó";
        }
        refreshStrip();
    }
    void refreshStrip(bool force = false)
    {
        if (!render || !options.sheets)
            return;
        if (!stripVisible) {
            if (stripSent)
                render->hideStrip();
            stripSent = false;
            canvas->setStrip({}, nullptr);
            return;
        }
        const double scale = screen->devicePixelRatio();
        const int rightLimit =
            panelVisible ? panelLayout(canvas->width(), canvas->height(), scale,
                                       double(mapping.sheetWidth) / std::max(1, mapping.sheetHeight)).panel.left()
                         : canvas->width();
        const QRect rect = audioStripRect(canvas->width(), canvas->height(),
                                          QRect(mapping.sheetX, mapping.sheetY, mapping.sheetWidth, mapping.sheetHeight),
                                          rightLimit, scale);
        AudioStripInfo info;
        info.name = audioFile;
        info.problem = audioProblem;
        info.loading = audioLoading;
        info.marks = audioMarks;
        info.seconds = clip ? clip->seconds() : 0;
        info.position = audioPosition;
        if (clip) {
            const int columns = audioWaveRect(rect.size(), scale).width();
            info.peaks = rect.size() == stripRect.size() && int(stripShown.peaks.size()) == columns && stripShown.name == info.name
                             ? stripShown.peaks
                             : audio::peaks(*clip, columns);
        }
        if (!force && stripSent && info == stripShown && rect == stripRect)
            return;
        canvas->setStrip(RECT{rect.left(), rect.top(), rect.right() + 1, rect.bottom() + 1},
                         [this](int phase, double x, double y, bool pen) { stripInput(phase, x, y, pen); });
        const QImage painted = paintAudioStrip(info, rect.size(), scale, appkit::theme::kFuera);
        const auto* bits = reinterpret_cast<const uint32_t*>(painted.constBits());
        render->setStrip(rect.x(), rect.y(), painted.width(), painted.height(),
                         std::vector<uint32_t>(bits, bits + size_t(painted.width()) * size_t(painted.height())));
        stripShown = std::move(info);
        stripRect = rect;
        stripSent = true;
    }

    // Guía por hoja (HU-119): por id de hoja; se pinta del tamaño de la hoja en pantalla y se
    // sube una vez (al calibrar, de nuevo).
    QHash<uint64_t, QPicture> sheetGuides;
    bool sheetGuidesVisible = true;
    bool testGuides = false; // --guias-de-prueba
    void sendSheetGuide(uint64_t id, const QPicture& guide)
    {
        if (!render)
            return;
        if (guide.isNull()) {
            render->setSheetGuide(id, 0, 0, {});
            return;
        }
        QImage painted(mapping.sheetWidth, mapping.sheetHeight, QImage::Format_ARGB32_Premultiplied);
        painted.fill(Qt::transparent);
        QPainter painter(&painted);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawPicture(0, 0, guide);
        painter.end();
        const auto* bits = reinterpret_cast<const uint32_t*>(painted.constBits());
        render->setSheetGuide(id, painted.width(), painted.height(),
                              std::vector<uint32_t>(bits, bits + size_t(painted.width()) * size_t(painted.height())));
    }
    void applySheetGuides()
    {
        if (!render)
            return;
        render->clearSheetGuides();
        for (auto it = sheetGuides.cbegin(); it != sheetGuides.cend(); ++it)
            sendSheetGuide(it.key(), it.value());
    }
    // --guias-de-prueba: a cada hoja que no tiene, una guía con su número y una caja girada.
    void addTestGuides()
    {
        for (size_t i = 0; i < order.ids.size(); ++i) {
            const uint64_t id = order.ids[i];
            if (sheetGuides.contains(id))
                continue;
            QPicture guide;
            QPainter painter(&guide);
            const double w = mapping.sheetWidth, h = mapping.sheetHeight;
            painter.setPen(QPen(appkit::theme::kGuiaSuave, 2));
            painter.drawLine(QPointF(0, h * 0.6), QPointF(w, h * 0.6));
            painter.setPen(QPen(appkit::theme::kGuia, 3));
            painter.translate(w / 2, h / 2);
            painter.rotate(15.0 * double(i));
            painter.drawRect(QRectF(-h * 0.2, -h * 0.15, h * 0.4, h * 0.3));
            painter.resetTransform();
            QFont font = painter.font();
            font.setPixelSize(int(h / 8));
            painter.setFont(font);
            painter.drawText(QRectF(0, 0, w * 0.2, h * 0.2), Qt::AlignCenter, QString::number(i + 1));
            painter.end();
            sheetGuides.insert(id, guide);
            sendSheetGuide(id, guide);
        }
    }

    // Guía de campo (HU-103): se pinta una vez, del tamaño de la hoja en pantalla.
    bool fieldGuide = false;
    void applyGuide()
    {
        if (!render)
            return;
        if (!fieldGuide) {
            render->setGuide(0, 0, {}, 0, 0);
            return;
        }
        const QImage guide = paintFieldGuide(mapping.sheetWidth, mapping.sheetHeight,
                                             mapping.sheetWidth / paper.spec().widthMm, screen->devicePixelRatio());
        const auto* bits = reinterpret_cast<const uint32_t*>(guide.constBits());
        const QColor ink = appkit::theme::kMarcaHoja;
        render->setGuide(guide.width(), guide.height(),
                         std::vector<uint32_t>(bits, bits + size_t(guide.width()) * size_t(guide.height())),
                         bgra(ink.red(), ink.green(), ink.blue()), appkit::theme::kGuiaCampoOpacidad);
    }

    // Exportar (HU-109, lienzo/Export.h): la copia del depósito de las hojas se toma con la
    // simulación detenida; dibujar y comprimir los PNG va en un hilo aparte. El panel dice
    // cuántas van y, al terminar, "exportado" unos segundos (exportNote).
    std::thread exporter;
    std::atomic<bool> exportCancel{false};
    int exportDone = -1, exportTotal = 0; // -1: no está exportando
    QString exportNote;
    QTimer exportNoteTimer;
    void exportFinished(const ExportResult& result, double ms, const QString& dir)
    {
        exportDone = -1;
        if (result.ok) {
            qInfo().noquote() << QStringLiteral("Exportar: %1 hojas en %2, en %3 s")
                                     .arg(result.sheets)
                                     .arg(QDir::toNativeSeparators(dir))
                                     .arg(ms / 1000, 0, 'f', 1);
            exportNote = QStringLiteral("exportado");
        } else {
            qWarning().noquote() << QStringLiteral("Exportar: no se pudo: %1").arg(result.error);
            exportNote = QStringLiteral("no se exportó");
        }
        exportNoteTimer.start();
        refreshSheetPanel();
    }

    // Panel de hojas (HU-114): se vuelve a pintar solo si cambió algo (la imagen la pinta Qt
    // acá; las miniaturas, el render). Lo llaman applyLayers (cada cambio de la pila o flip) y
    // un reloj, para el estado del guardado, el lápiz y el giro.
    bool panelVisible = false;
    QString panelHeading, panelName, panelNote; // HU-120: el encabezado sin toma
    PanelInfo panelShown;
    QRect panelRect;
    bool panelSent = false;
    QTimer panelTimer;
    void refreshSheetPanel(bool force = false)
    {
        if (!render || !options.sheets)
            return;
        if (!panelVisible) {
            if (panelSent)
                render->hidePanel();
            panelSent = false;
            return;
        }
        syncOrder();
        const int shown = flipping() && order.valid(shownSheet) ? shownSheet : order.active;
        // HU-89: dibujando en la fija, la tira sigue alrededor de la hoja de la que se vino.
        const int center = showingFixed() ? fixedReturnIndex() : shown;
        PanelInfo info;
        info.take = saver ? QFileInfo(saver->dir()).fileName() : QString();
        info.saved = !saver || (!saver->pending() && sim->unsavedSheets() == 0);
        if (!saver && !panelHeading.isEmpty()) { // HU-120
            info.heading = panelHeading;
            info.take = panelName;
            info.note = panelNote;
            info.showState = false;
        }
        if (exportDone >= 0) { // HU-109
            info.exportState = QStringLiteral("exportando %1/%2").arg(exportDone).arg(exportTotal);
            info.exportBusy = true;
        } else if (!exportNote.isEmpty()) {
            info.exportState = exportNote;
            info.exportBusy = exportNote != QStringLiteral("exportado");
        }
        info.number = lienzo::sheetNumber(order, fixedSheet, shown);
        info.role = roleOf(roles, order.ids[size_t(shown)]);
        info.total = flipCount(order, fixedSheet);
        info.strip = panelSlots(order, center, 3, roles, fixedSheet, circle);
        info.circle = circle;
        if (showingFixed())
            for (PanelSlot& s : info.strip)
                s.center = false;
        info.hasFixed = fixedIndex(order, fixedSheet) >= 0;
        info.onFixed = showingFixed();
        if (colorPencil >= 0) {
            info.lead = QString::fromUtf8(kColorPencilNames[size_t(colorPencil)].name);
            info.leadColor = QColor::fromRgb(QRgb(kColorPencilColors[size_t(colorPencil)]));
        } else {
            info.lead = QStringLiteral("%1 · %2 mm")
                            .arg(QString::fromLatin1(media.activeName()),
                                 QString::number(sim->lead().diameter / 100.0, 'f', 2).replace(QLatin1Char('.'), QLatin1Char(',')));
        }
        const auto color = [](const Tint& t) {
            QColor c = QColor::fromRgb(QRgb(t.color));
            c.setAlphaF(t.opacity);
            return c;
        };
        for (auto it = lightTable.before.rbegin(); it != lightTable.before.rend(); ++it)
            info.light.push_back(color(*it));
        for (const Tint& t : lightTable.after)
            info.light.push_back(color(t));
        info.keysOnly = lightTable.keysOnly && !info.light.empty(); // HU-93
        info.degrees = int(std::lround(viewDegrees)) % 360;
        info.zoom = int(std::lround(viewScale * 100));
        const PanelLayout layout = panelLayout(canvas->width(), canvas->height(), screen->devicePixelRatio(),
                                               double(mapping.sheetWidth) / std::max(1, mapping.sheetHeight));
        if (!force && panelSent && info == panelShown && layout.panel == panelRect)
            return;
        std::vector<PanelThumb> thumbs;
        for (size_t i = 0; i < info.strip.size() && i < layout.thumbs.size(); ++i) {
            const QRect r = layout.thumbs[i].translated(layout.panel.topLeft());
            thumbs.push_back({info.strip[i].id, RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()}});
        }
        const QImage painted = paintPanel(info, layout, QColor::fromRgb(QRgb(options.paperColor)), appkit::theme::kFuera);
        const auto* bits = reinterpret_cast<const uint32_t*>(painted.constBits());
        render->setPanel(layout.panel.x(), layout.panel.y(), painted.width(), painted.height(),
                         std::vector<uint32_t>(bits, bits + size_t(painted.width()) * size_t(painted.height())),
                         std::move(thumbs));
        panelShown = std::move(info);
        panelRect = layout.panel;
        panelSent = true;
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
        // HU-89: desde la fija, la primera flecha vuelve a la hoja de la que se vino.
        const bool fromFixed = starting && fixedIndex(o, fixedSheet) == o.active;
        if (fromFixed && fixedReturnIndex() < 0)
            return; // la única hoja es la fija
        if (starting) {
            releasedTo = 0;
            shownSheet = fromFixed ? fixedReturnIndex() : o.active;
            canvas->setPenBlocked(true);
        }
        if (pressedKey && std::find(flipKeys.begin(), flipKeys.end(), pressedKey) == flipKeys.end())
            flipKeys.push_back(pressedKey);
        const int next = fromFixed ? shownSheet : flipStep(o, fixedSheet, shownSheet, step, circle); // -1: en el extremo se queda
        const bool moved = next >= 0 && (fromFixed || next != shownSheet);
        if (moved) {
            shownSheet = next;
            qInfo().noquote() << QStringLiteral("Flip: hoja %1 de %2")
                                     .arg(lienzo::sheetNumber(o, fixedSheet, next))
                                     .arg(flipCount(o, fixedSheet));
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
            qInfo().noquote() << QStringLiteral("Flip: activa la hoja %1 de %2")
                                     .arg(lienzo::sheetNumber(o, fixedSheet, target))
                                     .arg(flipCount(o, fixedSheet));
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

    // Hoja fija (HU-89, lienzo/FixedSheet.h): por id, como los roles. fixedReturn: la hoja de
    // la que se vino a dibujar en ella (Ctrl+F), a la que se vuelve.
    uint64_t fixedSheet = 0, fixedReturn = 0;
    bool circle = false; // HU-94: pila en círculo (se guarda en la toma)
    bool showingFixed() const { return !flipping() && order.valid(order.active) && fixedIndex(order, fixedSheet) == order.active; }
    int fixedReturnIndex() const
    {
        const int i = order.indexOf(fixedReturn);
        return i >= 0 && i != fixedIndex(order, fixedSheet) ? i : flipEdge(order, fixedSheet, false);
    }
    // "la hoja 3 de 12" o "la hoja fija", para el log.
    QString sheetLabel(int index) const
    {
        const int n = lienzo::sheetNumber(order, fixedSheet, index);
        return n ? QStringLiteral("hoja %1 de %2").arg(n).arg(flipCount(order, fixedSheet)) : QStringLiteral("hoja fija");
    }

    // Zoom y corrimiento de la vista (HU-101, HU-102): el corrimiento en píxeles del cliente.
    double viewScale = 1, viewPanX = 0, viewPanY = 0;
    ViewRotation rotation() const
    {
        return {viewDegrees, mapping.sheetX + mapping.sheetWidth / 2.0, mapping.sheetY + mapping.sheetHeight / 2.0,
                viewScale, viewPanX, viewPanY};
    }
    void applyView()
    {
        const ViewRotation r = rotation();
        sim->setRotation(r);
        if (render)
            render->setRotation(r);
    }

    // persist: guarda el giro en config.json (HU-13); el gesto guarda solo al soltar.
    void setViewRotation(double degrees, bool persist = true)
    {
        viewDegrees = ViewRotation::normalized(degrees);
        applyView();
        if (persist)
            config->setValue(QStringLiteral("viewRotation"), viewDegrees);
    }

    // HU-101: la escala `to` con el punto (x, y) del cliente quieto.
    void setViewScale(double to, double x, double y)
    {
        const ViewRotation v = rotation().zoomedAt(to, x, y).clampedPan(canvas->width(), canvas->height());
        viewScale = v.scale;
        viewPanX = v.panX;
        viewPanY = v.panY;
        applyView();
    }
    // Hacia dónde va el zoom: la punta, si estuvo sobre la hoja hace poco; si no, el centro de
    // la hoja como se ve.
    void zoomAnchor(double& x, double& y) const
    {
        const ViewRotation r = rotation();
        double px, py;
        if (canvas->recentPen(px, py) && px >= mapping.sheetX && px < mapping.sheetX + mapping.sheetWidth &&
            py >= mapping.sheetY && py < mapping.sheetY + mapping.sheetHeight) {
            x = px;
            y = py;
            return;
        }
        x = r.cx + r.panX;
        y = r.cy + r.panY;
    }
    void zoomBy(double factor, const char* how)
    {
        double x, y;
        zoomAnchor(x, y);
        setViewScale(viewScale * factor, x, y);
        qInfo().noquote() << QStringLiteral("Zoom %1 % (%2)").arg(std::lround(viewScale * 100)).arg(QString::fromUtf8(how));
    }
    // Pan (HU-102): Espacio + arrastrar mueve la vista lo mismo que la punta; se anota al soltar.
    double panStartX = 0, panStartY = 0, panPointerX = 0, panPointerY = 0;
    void panGesture(int phase, double x, double y)
    {
        if (phase == 0) {
            panStartX = viewPanX;
            panStartY = viewPanY;
            panPointerX = x;
            panPointerY = y;
            return;
        }
        ViewRotation v = rotation();
        v.panX = panStartX + x - panPointerX;
        v.panY = panStartY + y - panPointerY;
        v = v.clampedPan(canvas->width(), canvas->height());
        viewPanX = v.panX;
        viewPanY = v.panY;
        applyView();
        if (phase == 2)
            qInfo().noquote() << QStringLiteral("Vista corrida a (%1, %2) px").arg(std::lround(viewPanX)).arg(std::lround(viewPanY));
    }

    // Ctrl+0 (HU-116): giro a 0°, 100 % y centrada.
    void resetView()
    {
        viewScale = 1;
        viewPanX = viewPanY = 0;
        setViewRotation(0);
    }
    // La vista y los roles como los guarda toma.json.
    TakeView takeView() const
    {
        return {viewDegrees, viewScale, mapping.sheetWidth ? viewPanX / mapping.sheetWidth : 0,
                mapping.sheetHeight ? viewPanY / mapping.sheetHeight : 0, roles,
                fixedIndex(order, fixedSheet) >= 0 ? fixedSheet : 0, circle, audioFile, audioMarks};
    }
    int ringMode = -1; // el último modo de la rueda que se vio (HU-101: se anota al cambiar)
    // HU-95: el modo "Hojas" de la rueda (-1: sin hojas) y sus pasos. El flip de la rueda va como
    // una tecla más (kRingKey, que no es una tecla real): termina al levantar el dedo.
    int ringSheetsMode = -1;
    tabletinput::RingStepper ringSheets{30};
    static constexpr UINT kRingKey = 0x10000;
    void ringFlip(int step)
    {
        const UINT pressed = pressedKey;
        pressedKey = kRingKey;
        flip(step);
        pressedKey = pressed;
    }

    // Lápiz de color (HU-99): -1 el grafito, 0..3 los colores; queda guardado.
    int colorPencil = -1;
    void setColorPencil(int color, bool persist = true)
    {
        colorPencil = std::clamp(color, -1, drymedia::kColorCount - 1);
        sim->setColorPencil(colorPencil);
        if (persist)
            config->setValue(QStringLiteral("pencil"), colorPencil);
        if (colorPencil < 0)
            qInfo() << "Lápiz:" << media.activeName();
        else
            qInfo().noquote() << QStringLiteral("Lápiz: %1 (%2)")
                                     .arg(QString::fromUtf8(kColorPencilNames[size_t(colorPencil)].name),
                                          QString::fromUtf8(kColorPencilNames[size_t(colorPencil)].use));
        refreshSheetPanel();
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
        if (options.colorPencils) // HU-99
            colorPencil = std::clamp(config->value(QStringLiteral("pencil"), -1).toInt(), -1, drymedia::kColorCount - 1);
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
        const ViewRotation r = rotation(); // alrededor del centro de la hoja como se ve
        return std::atan2(y - r.cy - r.panY, x - r.cx - r.panX) * 180.0 / 3.14159265358979323846;
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
        markNumber = 0; // la marca va en la esquina de la hoja nueva
        applyGuide();   // y la guía, del tamaño nuevo
        applySheetGuides(); // HU-119: las de las hojas también
        refreshStrip(true);
        applyLayers();
        if (!area)
            render->setOverlay(true);
        if (wasRunning)
            sim->start();
        refreshSheetPanel(true); // la hoja cambió de forma: otras miniaturas
        qInfo() << "Hoja reubicada en" << mapping.sheetWidth << "x" << mapping.sheetHeight << "px desde"
                << mapping.sheetX << mapping.sheetY;
        if (onSheetChanged)
            onSheetChanged();
    }

    void startCalibration()
    {
        picker->hide();
        if (pencilPicker)
            pencilPicker->hide();
        if (penMenu)
            penMenu->hide();
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
        // HU-117: entra en la pantalla y va centrada en ella (no la alcanza el lápiz: es para leer).
        shortcutsOverlay->setSheet(QApplication::applicationName(), shortcuts.sheet(), shell->size());
        const QPoint center = shell->rect().center();
        shortcutsOverlay->move(shell->mapToGlobal(center - QPoint(shortcutsOverlay->width() / 2, shortcutsOverlay->height() / 2)));
        shortcutsOverlay->show();
        shortcutsOverlay->raise();
        shortcutsOverlay->activateWindow();
        shortcutsOverlay->setFocus();
        qInfo() << "Atajos: abiertos";
    }

    // Botón lateral con la punta en (x, y) del lienzo: el menú del lápiz (HU-116) o lo de la app.
    void stylusButton(int x, int y)
    {
        if (!penMenu) {
            if (onStylusButton)
                onStylusButton();
            return;
        }
        if (penMenu->isVisible()) { // la punta fuera de la hoja: el menú no recibió el botón
            penMenu->hide();
            qInfo() << "Menú del lápiz: cerrado (botón)";
            focusCanvas();
            return;
        }
        std::vector<QColor> colors; // el grafito y los colores, solo con lápices de color (HU-99)
        if (options.colorPencils) {
            colors.push_back(QColor(0x3A, 0x3A, 0x3E));
            for (uint32_t c : kColorPencilColors)
                colors.push_back(QColor::fromRgb(QRgb(c)));
        }
        penMenu->setPencils(std::move(colors), colorPencil + 1);
        const QRect sheet = toQt({mapping.sheetX, mapping.sheetY, mapping.sheetWidth, mapping.sheetHeight});
        const double dpr = screen->devicePixelRatio();
        const QPoint tip(int((x - mapping.sheetX) / dpr), int((y - mapping.sheetY) / dpr));
        penMenu->open(QRect(shell->mapToGlobal(sheet.topLeft()), sheet.size()), tip);
        qInfo() << "Menú del lápiz: abierto";
    }

    // HU-100: el grafito y los cuatro colores, centrado sobre la hoja (al alcance del lápiz).
    void togglePencilPicker()
    {
        if (pencilPicker->isVisible()) {
            pencilPicker->hide();
            qInfo() << "Selector de lápices: cerrado";
            focusCanvas();
            return;
        }
        pencilPicker->setPencils(media.lead(), colorPencil, mapping.pixelsPerCellX, paper.cellsPerMm(), options.profile,
                                 options.paperColor);
        qInfo() << "Selector de lápices: abierto";
        showOverlay(pencilPicker.get());
    }

    void togglePicker()
    {
        if (pencilPicker) {
            togglePencilPicker();
            return;
        }
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
            if (colorPencil >= 0) // HU-100: el lápiz de color activo
                swprintf(text, 768, L"lápiz %hs (%hs) (F5)   ·   %.2f mm   ·   costado %ls (I)",
                         kColorPencilNames[size_t(colorPencil)].name, kColorPencilNames[size_t(colorPencil)].use,
                         drymedia::Medium::colored(colorPencil).leadDiameterMm, tilt ? L"sí" : L"no");
            else if (options.fixedGrade >= 0) // un solo grafito (HU-98): no se calibra acá
                swprintf(text, 768, L"grafito %hs%ls   ·   %.2f mm   ·   costado %ls (I)", media.activeName(),
                         options.colorPencils ? L" (F5)" : L"", lead.diameter / 100.0, tilt ? L"sí" : L"no");
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
            if (lightTable.keysOnly && !lightTable.before.empty()) // HU-93
                out += L"   ·   mesa de luz: solo keys (Ctrl+L)";
        }
        if (options.zoom) { // HU-101
            swprintf(text, 768, L"   ·   zoom %d %% (+ −)", int(std::lround(viewScale * 100)));
            out += text;
        }
        if (options.paperTexture)
            swprintf(text, 768, L"   ·   textura %d %% (Ctrl+[ ])%ls", texture, warning);
        else
            swprintf(text, 768, L"%ls", warning);
        out += takeWarning;
        if (saver) // HU-86
            if (const QString error = saver->stats().error; !error.isEmpty())
                out += QStringLiteral("\nToma: no se pudo guardar (%1).").arg(error).toStdWString();
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
    // Los modos de la rueda (botón del centro): girar y, con zoom, zoom (HU-101); los demás giran.
    std::vector<std::string> ringModes{"Girar la vista"};
    if (d->options.zoom)
        ringModes.push_back("Zoom");
    if (d->options.sheets) { // HU-95
        d->ringSheetsMode = int(ringModes.size());
        ringModes.push_back("Hojas");
    }
    d->canvas = std::make_unique<CanvasWindow>(reinterpret_cast<HWND>(shell.winId()), d->queue, dispatch,
                                               std::move(ringModes));
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
    if (d->options.sheets) { // panel de hojas (HU-114): el guardado, el lápiz y el giro cambian solos
        d->playTimer.setInterval(33); // HU-107: el cursor del audio, a 30 cuadros por segundo
        QObject::connect(&d->playTimer, &QTimer::timeout, [impl = d.get()] { impl->audioTick(); });
        d->exportNoteTimer.setSingleShot(true); // HU-109: "exportado", unos segundos
        d->exportNoteTimer.setInterval(4000);
        QObject::connect(&d->exportNoteTimer, &QTimer::timeout, [impl = d.get()] {
            impl->exportNote.clear();
            impl->refreshSheetPanel();
        });
        d->panelTimer.setInterval(200);
        QObject::connect(&d->panelTimer, &QTimer::timeout, [impl = d.get()] { impl->refreshSheetPanel(); });
        d->panelTimer.start();
    }

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
    if (d->options.zoom) // HU-102: con el zoom viene el pan
        d->canvas->setOnPanGesture([impl = d.get()](int phase, double x, double y) { impl->panGesture(phase, x, y); });
    // Rueda táctil (HU-74): giro libre, sin snap; se guarda al levantar el dedo.
    d->canvas->setOnRing([impl = d.get()](double degrees, bool ended, int mode) {
        if (mode != impl->ringMode) {
            impl->ringMode = mode;
            qInfo() << "Rueda: modo" << mode;
            impl->ringSheets.reset(); // HU-95: cambiar de modo a mitad de un flip lo suelta
            impl->keyUp(Impl::kRingKey);
        }
        // HU-95: en el modo "Hojas", la rueda flipea como mantener la flecha: una hoja cada 30°
        // (sentido horario = la siguiente); al levantar el dedo, la que se ve queda activa.
        if (mode == impl->ringSheetsMode) {
            for (int steps = impl->ringSheets.feed(degrees); steps != 0; steps -= steps > 0 ? 1 : -1)
                impl->ringFlip(steps > 0 ? 1 : -1);
            if (ended) {
                impl->ringSheets.reset();
                impl->keyUp(Impl::kRingKey);
            }
            return;
        }
        // HU-101: en el segundo modo (el botón del centro), zoom continuo: un cuarto de vuelta
        // duplica o reduce a la mitad.
        if (impl->options.zoom && mode == 1) {
            if (degrees != 0) {
                double x, y;
                impl->zoomAnchor(x, y);
                impl->setViewScale(impl->viewScale * std::pow(2.0, degrees / 90.0), x, y);
                impl->ringTurned = true;
            }
            if (ended && impl->ringTurned) {
                impl->ringTurned = false;
                qInfo().noquote() << QStringLiteral("Zoom %1 % (rueda)").arg(std::lround(impl->viewScale * 100));
            }
            return;
        }
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

    d->canvas->setOnStylusButton([impl = d.get()](int x, int y) { impl->stylusButton(x, y); });
    d->picker = std::make_unique<LeadPicker>(&shell);
    d->picker->setPaper(d->paper.cellsPerMm(), d->options.profile);
    d->picker->onPick = [impl = d.get()](int grade) {
        impl->setActive(grade);
        impl->focusCanvas();
    };
    d->picker->onClose = [impl = d.get()] { impl->focusCanvas(); };
    if (d->options.colorPencils) { // HU-100
        d->pencilPicker = std::make_unique<PencilPicker>(&shell);
        d->pencilPicker->onPick = [impl = d.get()](int color) {
            impl->setColorPencil(color);
            impl->focusCanvas();
        };
        d->pencilPicker->onClose = [impl = d.get()] {
            qInfo() << "Selector de lápices: cerrado";
            impl->focusCanvas();
        };
    }
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
    d->stopAudio();
    if (d->audioLoader.joinable()) // antes que relay, al que avisa
        d->audioLoader.join();
    d->exportCancel = true; // HU-109: corta entre hojas
    if (d->exporter.joinable())
        d->exporter.join();
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
    d->onStylusButton = std::move(callback);
}

// El nombre del atajo de cada lápiz (HU-99): -1 el grafito, 0..3 los colores.
static QString pencilShortcutName(int color)
{
    if (color < 0)
        return QStringLiteral("Grafito");
    const ColorPencilName& n = kColorPencilNames[size_t(color)];
    return QStringLiteral("Lápiz %1 (%2)").arg(QString::fromUtf8(n.name), QString::fromUtf8(n.use));
}

void Lienzo::setPenMenu(std::vector<PenMenuAction> actions)
{
    for (PenMenuAction& a : actions)
        a.key = d->shortcuts.keyOf(a.shortcut);
    if (!d->penMenu) {
        d->shortcuts.setGroup(kGroupGestures); // HU-117: en la lista de atajos
        d->shortcuts.note(QStringLiteral("Botón del lápiz"), QStringLiteral("Menú del lápiz"));
        d->penMenu = std::make_unique<PenMenu>(d->shell);
        Impl* impl = d.get();
        d->penMenu->onPencil = [impl](int index) {
            qInfo().noquote() << QStringLiteral("Menú del lápiz: %1").arg(pencilShortcutName(index - 1));
            impl->shortcuts.run(pencilShortcutName(index - 1));
            impl->focusCanvas();
        };
        d->penMenu->onAction = [impl](int index) {
            const PenMenuAction& a = impl->penMenu->actions()[size_t(index)];
            qInfo().noquote() << QStringLiteral("Menú del lápiz: %1").arg(a.shortcut);
            impl->shortcuts.run(a.shortcut);
        };
        d->penMenu->onDismiss = [impl](const char* how) {
            qInfo().noquote() << QStringLiteral("Menú del lápiz: cerrado (%1)").arg(QString::fromUtf8(how));
            impl->focusCanvas();
        };
    }
    d->penMenu->setActions(std::move(actions));
}

void Lienzo::registerShortcuts()
{
    appkit::Shortcuts& keys = d->shortcuts;
    Impl* impl = d.get();
    MediaFile& media = d->media;
    // Grupos de la lista de atajos (Ctrl+, HU-78), en el orden de la mesa "Atajos" (HU-117);
    // la app ordena los suyos con setOrder.
    keys.setOrder({kGroupLead, kGroupSheet, kGroupView, kGroupGestures, kGroupScreen});

    keys.setGroup(kGroupLead);
    const bool calibrable = d->options.fixedGrade < 0; // con un solo grafito (HU-98), nada de esto
    if (calibrable || d->options.colorPencils) // HU-67; HU-100: el grafito y los colores
        keys.add(QStringLiteral("Selector de lápices"), {{VK_F5}}, [impl] { impl->togglePicker(); });
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

    if (d->options.colorPencils) { // HU-99: 1 el grafito, 2 a 5 los colores
        keys.setGroup(kGroupLead);
        keys.add(pencilShortcutName(-1), {{'1'}, {VK_NUMPAD1}}, [impl] { impl->setColorPencil(-1); });
        for (int c = 0; c < drymedia::kColorCount; ++c) {
            keys.add(pencilShortcutName(c), {{UINT('2' + c)}, {UINT(VK_NUMPAD2 + c)}},
                     [impl, c] { impl->setColorPencil(c); });
            keys.joinWithPrevious(QStringLiteral("Grafito y colores")); // HU-117: una fila (F5 dice cuál es cuál)
        }
    }

    keys.setGroup(kGroupView);
    if (!d->options.colorPencils) { // los números son de la vista (HU-40); la dureza se elige con F5
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
    // HU-116: giro a 0° (y, con zoom y pan, HU-101 y HU-102, la hoja entera y centrada).
    keys.add(QStringLiteral("Vista inicial"), {{'0', true}}, [impl] {
        impl->resetView();
        qInfo() << "Vista inicial: rotada 0 °, zoom 100 %";
    });
    if (d->options.zoom) { // HU-101
        keys.add(QStringLiteral("Acercar"), {{VK_OEM_PLUS}, {VK_ADD}}, [impl] { impl->zoomBy(ViewRotation::kZoomStep, "+"); });
        keys.add(QStringLiteral("Alejar"), {{VK_OEM_MINUS}, {VK_SUBTRACT}},
                 [impl] { impl->zoomBy(1 / ViewRotation::kZoomStep, "−"); });
        keys.joinWithPrevious(QStringLiteral("Zoom"));
        keys.add(QStringLiteral("Zoom 100 %"), {{'1', true}}, [impl] {
            impl->viewScale = 1;
            impl->viewPanX = impl->viewPanY = 0;
            impl->applyView();
            qInfo() << "Zoom 100 % (Ctrl+1)";
        });
    }
    // Los gestos (HU-117: aparte, en chips punteados), en el orden de los modos de la rueda.
    keys.setGroup(kGroupGestures);
    keys.note(QStringLiteral("Rueda"), QStringLiteral("Girar la vista")); // HU-74
    if (d->options.zoom)
        keys.note(QStringLiteral("Rueda, 2.º modo"), QStringLiteral("Zoom")); // HU-101
    if (d->options.sheets)
        keys.note(d->options.zoom ? QStringLiteral("Rueda, 3.er modo") : QStringLiteral("Rueda, 2.º modo"),
                  QStringLiteral("Pasar hojas")); // HU-95
    if (d->options.zoom)
        keys.note(QStringLiteral("Espacio + arrastrar"), QStringLiteral("Mover la vista")); // HU-102
    keys.note(QStringLiteral("Shift + arrastrar"), QStringLiteral("Girar libre")); // HU-79

    keys.setGroup(kGroupScreen);
    keys.add(QStringLiteral("Atajos"), {{VK_OEM_COMMA, true}}, [impl] { impl->toggleShortcuts(); }); // HU-78
    keys.add(QStringLiteral("Latencia y herramienta"), {{VK_F3}}, [impl] { impl->render->toggleOverlay(); });
    keys.add(QStringLiteral("Calibrar el área útil"), {{VK_F9}}, [impl] { impl->startCalibration(); }); // HU-66
    keys.add(QStringLiteral("Monitor siguiente"), {{VK_F10}}, [impl] { impl->nextScreen(); });          // HU-66
    keys.add(QStringLiteral("Imagen de pantalla"), {{VK_F12}}, [impl] { impl->saveScreenImage(); });    // diagnóstico
    keys.add(QStringLiteral("Salir"), {{'Q', true}}, [] { QApplication::quit(); }); // también Alt+F4

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
    // --guias-de-prueba (HU-119): una guía distinta en cada hoja, para medir el flip con guías.
    if (args.contains(QStringLiteral("--guias-de-prueba")) && d->options.sheets) {
        d->testGuides = true;
        d->applyLayers();
        qInfo() << "Guías de prueba en las" << sheetCount() << "hojas";
    }
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
    d->sim->stop(); // pasa al guardado lo que quedaba (HU-86)
    if (d->saver) {
        const Stopwatch time;
        d->saver->flush();
        const TakeSaver::Stats s = d->saver->stats();
        qInfo().noquote() << QStringLiteral("Toma: guardada al cerrar en %1 ms · en la sesión %2 hojas y %3 toma.json · "
                                            "copia en la simulación de hasta %4 ms (se corta al dibujar)%5")
                                 .arg(time.ms(), 0, 'f', 0)
                                 .arg(s.sheets)
                                 .arg(s.indexes)
                                 .arg(d->sim->autosaveCopyMaxMs(), 0, 'f', 1)
                                 .arg(s.error.isEmpty() ? QString() : QStringLiteral(" · error: ") + s.error);
    }
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

bool Lienzo::exportTake()
{
    if (!d->saver) {
        qInfo() << "Exportar: no hay toma";
        return false;
    }
    if (d->exportDone >= 0) {
        qInfo() << "Exportar: ya se está exportando";
        return false;
    }
    if (d->exporter.joinable())
        d->exporter.join(); // la anterior ya terminó (avisó)
    ExportJob job;
    job.dir = QDir(d->saver->dir()).filePath(QString::fromLatin1(kExportFolder));
    job.paper = d->paper.spec();
    job.paperColor = d->options.paperColor;
    if (!d->audioFile.isEmpty()) {
        job.audio = QDir(d->saver->dir()).filePath(d->audioFile);
        job.marks = d->audioMarks;
    }
    // La copia del depósito de cada hoja, con el papel quieto (como al guardar).
    const bool wasRunning = d->running;
    if (wasRunning)
        d->sim->stop();
    const Stopwatch time;
    const SheetOrder& order = d->sheets.order();
    for (int i = 0; i < order.count; ++i)
        job.sheets.push_back({d->sheets.serialize(d->paper, i), lienzo::sheetNumber(order, d->fixedSheet, i),
                              roleOf(d->roles, order.ids[size_t(i)])});
    const double copyMs = time.ms();
    if (wasRunning)
        d->sim->start();
    qInfo().noquote() << QStringLiteral("Exportar: %1 hojas copiadas en %2 ms").arg(job.sheets.size()).arg(copyMs, 0, 'f', 0);
    d->exportDone = 0;
    d->exportTotal = int(job.sheets.size());
    d->exportNote.clear();
    d->exportCancel = false;
    d->refreshSheetPanel();
    d->exporter = std::thread([impl = d.get(), job = std::move(job)] {
        const Stopwatch time;
        const ExportResult result = lienzo::exportTake(
            job,
            [impl](int done, int) {
                QMetaObject::invokeMethod(
                    &impl->relay,
                    [impl, done] {
                        if (impl->exportDone >= 0)
                            impl->exportDone = done;
                        impl->refreshSheetPanel();
                    },
                    Qt::QueuedConnection);
            },
            &impl->exportCancel);
        const double ms = time.ms();
        QMetaObject::invokeMethod(
            &impl->relay, [impl, result, ms, dir = job.dir] { impl->exportFinished(result, ms, dir); }, Qt::QueuedConnection);
    });
    return true;
}

bool Lienzo::exporting() const
{
    return d->exportDone >= 0;
}

bool Lienzo::saveTake(const QString& dir)
{
    const bool wasRunning = d->running;
    if (wasRunning)
        d->sim->stop();
    const Stopwatch time;
    // La imagen de la activa es la de pantalla; las demás, las que dejó cada una al salir.
    const uint64_t activeId = d->sheets.order().activeId();
    std::vector<uint32_t> region(size_t(d->mapping.sheetWidth) * size_t(d->mapping.sheetHeight));
    {
        std::lock_guard lock(d->image.mutex);
        for (int y = 0; y < d->mapping.sheetHeight; ++y)
            std::copy_n(d->image.pixels.data() + size_t(d->mapping.sheetY + y) * size_t(d->image.width) + size_t(d->mapping.sheetX),
                        d->mapping.sheetWidth, region.data() + size_t(y) * size_t(d->mapping.sheetWidth));
    }
    const PackedImage activeImage = PackedImage::pack(d->mapping.sheetWidth, d->mapping.sheetHeight, region.data());
    const TakeResult result = lienzo::saveTake(dir, d->paper, d->sheets, d->takeView(), [&](uint64_t id) {
        return id == activeId ? activeImage : d->images.get(id);
    });
    const int count = d->sheets.count(), active = d->sheets.active();
    if (wasRunning)
        d->sim->start();
    if (result.ok)
        qInfo().noquote() << QStringLiteral("Toma guardada en %1: %2 hojas, activa la %3, en %4 ms")
                                 .arg(dir)
                                 .arg(count)
                                 .arg(active + 1)
                                 .arg(time.ms(), 0, 'f', 0);
    else
        qWarning().noquote() << QStringLiteral("Toma: no se pudo guardar en %1: %2").arg(dir, result.error);
    return result.ok;
}

bool Lienzo::openTake(const QString& dir)
{
    const bool wasRunning = d->running;
    if (wasRunning)
        d->sim->stop();
    if (d->saver)
        d->saver->flush(); // lo de la toma anterior, escrito antes de reemplazarla
    const Stopwatch time;
    const std::vector<uint64_t> before = d->sheets.order().ids;
    TakeView view;
    std::vector<std::pair<uint64_t, PackedImage>> saved;
    const TakeResult result = lienzo::openTake(dir, d->paper, d->sheets, view, &saved);
    if (!result.ok) {
        if (wasRunning)
            d->sim->start();
        qInfo().noquote() << QStringLiteral("Toma: no se abrió %1: %2").arg(dir, result.error);
        return false;
    }
    // Las texturas de las hojas de antes ya no sirven (los ids se repiten entre tomas).
    for (const uint64_t id : before)
        d->render->dropSheet(id);
    d->images.clear();
    // La imagen de cada hoja que no es la activa, para el render y para volver a guardarla:
    // la guardada si es del tamaño de la hoja en esta pantalla; si no, redibujada.
    const SheetOrder& order = d->sheets.order();
    int redrawn = 0;
    for (int i = 0; i < order.count; ++i) {
        if (i == order.active)
            continue;
        const uint64_t id = order.ids[size_t(i)];
        RetiredSheet sheet{id, d->mapping.sheetWidth, d->mapping.sheetHeight, {}};
        const auto it = std::find_if(saved.begin(), saved.end(), [id](const auto& p) { return p.first == id; });
        if (it != saved.end() && it->second.width == sheet.width && it->second.height == sheet.height &&
            it->second.unpack(sheet.pixels)) {
            d->images.putPacked(id, std::move(it->second));
        } else {
            sheet.pixels = d->renderSheet(i);
            d->images.put(id, sheet.width, sheet.height, sheet.pixels);
            ++redrawn;
        }
        std::lock_guard lock(d->image.mutex);
        d->image.retired.push_back(std::move(sheet));
    }
    d->takeWarning.clear();
    if (!result.blank.empty()) {
        QStringList numbers;
        for (const auto& [index, reason] : result.blank)
            numbers << QString::number(index + 1);
        d->takeWarning = QStringLiteral("\nToma: %1 %2 en blanco (archivo dañado o faltante; ver el log).")
                             .arg(numbers.size() == 1 ? QStringLiteral("la hoja") : QStringLiteral("las hojas"),
                                  numbers.join(QStringLiteral(", ")))
                             .toStdWString();
    }
    d->viewDegrees = ViewRotation::normalized(view.degrees);
    if (d->options.zoom) { // HU-101, HU-102
        d->viewScale = std::clamp(view.zoom, ViewRotation::kMinScale, ViewRotation::kMaxScale);
        d->viewPanX = view.panX * d->mapping.sheetWidth;
        d->viewPanY = view.panY * d->mapping.sheetHeight;
    }
    d->roles = view.roles;
    d->fixedSheet = view.fixed; // HU-89
    d->fixedReturn = 0;
    d->circle = view.circle; // HU-94
    d->audioFile = view.audio; // HU-106
    d->audioMarks = view.marks; // HU-108
    d->loadAudio(dir);
    d->createSimulation(); // con la pila nueva (y la vista); pinta la activa
    d->applyLayers();
    d->refreshStrip(true); // HU-106: el audio de esta toma
    if (wasRunning)
        d->sim->start();
    qInfo().noquote() << QStringLiteral("Toma abierta de %1: %2 hojas, activa la %3, %4 redibujadas, %5 en blanco, en %6 ms")
                             .arg(dir)
                             .arg(order.count)
                             .arg(order.active + 1)
                             .arg(redrawn)
                             .arg(result.blank.size())
                             .arg(time.ms(), 0, 'f', 0);
    return true;
}

QString Lienzo::useTake(const QString& dir)
{
    const QString target = takeFolder(dir, [this](const QString& folder) { return openTake(folder); });
    if (target != dir) {
        qWarning().noquote() << QStringLiteral("Toma: la de %1 no se pudo abrir; se usa %2").arg(dir, target);
        d->takeWarning += QStringLiteral("\nToma: la de %1 no se pudo abrir (ver el log); se usa %2.")
                              .arg(dir, target)
                              .toStdWString();
    }
    d->saver = std::make_unique<TakeSaver>(target, d->paper, &d->images);
    d->sim->setAutosave(d->saver.get());
    qInfo().noquote() << QStringLiteral("Toma: guardado automático en %1").arg(target);
    return target;
}

void Lienzo::setGuides(const QPicture& guides)
{
    d->guides = guides;
    d->sim->setGuides(d->guideLayer());
}

void Lienzo::setSheetGuide(int index, const QPicture& guide)
{
    const SheetOrder& order = d->syncOrder();
    if (!order.valid(index))
        return;
    const uint64_t id = order.ids[size_t(index)];
    if (guide.isNull())
        d->sheetGuides.remove(id);
    else
        d->sheetGuides.insert(id, guide);
    d->sendSheetGuide(id, guide);
}

void Lienzo::clearSheetGuides()
{
    d->sheetGuides.clear();
    if (d->render)
        d->render->clearSheetGuides();
}

void Lienzo::setSheetGuidesVisible(bool visible)
{
    d->sheetGuidesVisible = visible;
    if (d->render)
        d->render->setSheetGuidesVisible(visible);
    qInfo() << "Guías de las hojas:" << (visible ? "visibles" : "ocultas");
}

bool Lienzo::sheetGuidesVisible() const
{
    return d->sheetGuidesVisible;
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

SheetRole Lienzo::cycleActiveRole()
{
    const SheetOrder& order = d->syncOrder();
    const uint64_t id = order.activeId();
    const SheetRole role = nextRole(roleOf(d->roles, id));
    if (role == SheetRole::None)
        d->roles.erase(id);
    else
        d->roles[id] = role;
    d->sim->setRoles(d->roles);
    d->applyLayers();
    qInfo().noquote() << QStringLiteral("Rol: %1 · %2")
                             .arg(role == SheetRole::None ? QStringLiteral("sin rol") : roleName(role), d->sheetLabel(order.active));
    return role;
}

SheetRole Lienzo::activeRole() const
{
    const SheetOrder& order = d->syncOrder();
    return roleOf(d->roles, order.activeId());
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

void Lienzo::setPanelHeading(const QString& label, const QString& name, const QString& note)
{
    d->panelHeading = label;
    d->panelName = name;
    d->panelNote = note;
    d->refreshSheetPanel(true);
}

void Lienzo::setSheetPanel(bool visible)
{
    d->panelVisible = visible;
    d->refreshSheetPanel(true);
    d->refreshStrip(); // HU-106: el hueco para la franja cambia
}

bool Lienzo::setAudioFile(const QString& path)
{
    if (!d->saver) {
        qWarning() << "Audio: no hay toma donde copiarlo";
        return false;
    }
    const QString dir = d->saver->dir();
    const QFileInfo source(path);
    const QString name = QStringLiteral("audio.") + source.suffix().toLower();
    const QString target = QDir(dir).filePath(name);
    if (QFileInfo(target).canonicalFilePath() != source.canonicalFilePath()) {
        // El de antes se va (puede tener otra extensión); el nuevo se copia con nombre fijo.
        for (const QString& old : QDir(dir).entryList({QStringLiteral("audio.*")}, QDir::Files))
            QFile::remove(QDir(dir).filePath(old));
        if (!QFile::copy(path, target)) {
            qWarning().noquote() << QStringLiteral("Audio: no se pudo copiar %1 a %2").arg(path, dir);
            return false;
        }
    }
    d->audioFile = name;
    d->audioMarks.clear(); // las de otro audio no sirven
    d->loadAudio(dir);
    d->sim->setAudio(d->audioFile, d->audioMarks);
    d->refreshStrip(); // "leyendo…"
    qInfo().noquote() << QStringLiteral("Audio: %1 copiado a la toma como %2").arg(source.fileName(), name);
    d->refreshStrip();
    return d->audioProblem.isEmpty();
}

void Lienzo::togglePlay()
{
    d->togglePlay();
}

void Lienzo::rewindAudio()
{
    d->rewindAudio();
}

void Lienzo::markAudio()
{
    d->markAtCursor();
}

void Lienzo::removeAudioMark()
{
    d->removeNearestMark();
}

bool Lienzo::audioPlaying() const
{
    return d->player.playing();
}

QString Lienzo::audioFile() const
{
    return d->audioFile;
}

void Lienzo::setAudioStrip(bool visible)
{
    d->stripVisible = visible;
    d->refreshStrip(true);
    qInfo() << "Franja de audio:" << (visible ? "visible" : "oculta");
}

bool Lienzo::audioStrip() const
{
    return d->stripVisible;
}

bool Lienzo::sheetPanel() const
{
    return d->panelVisible;
}

void Lienzo::setTimingChart(bool visible)
{
    d->timingChart = visible;
    d->applyLayers();
    qInfo() << "Carta de timing:" << (visible ? "visible en las keys" : "oculta");
}

bool Lienzo::timingChart() const
{
    return d->timingChart;
}

void Lienzo::setFieldGuide(bool visible)
{
    d->fieldGuide = visible;
    d->applyGuide();
    qInfo() << "Guía de campo:" << (visible ? "visible" : "oculta");
}

bool Lienzo::fieldGuide() const
{
    return d->fieldGuide;
}

void Lienzo::setLightTable(const LightTable& table)
{
    d->lightTable = table;
    d->applyLayers();
    const SheetOrder& order = d->syncOrder();
    if (table.keysOnly && !table.before.empty() && lightTableLayers(order, table, d->roles, d->fixedSheet, d->circle).empty())
        qInfo().noquote() << QStringLiteral("Mesa de luz: no hay keys alrededor de la hoja %1").arg(order.active + 1);
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

bool Lienzo::moveActiveSheet(int step)
{
    d->beforeSheetChange();
    const SheetOrder& order = d->syncOrder();
    if (fixedIndex(order, d->fixedSheet) == order.active)
        return false; // HU-89: la fija no se mueve
    const int to = flipStep(order, d->fixedSheet, order.active, step); // pasa por encima de la fija
    if (to < 0)
        return false;
    moveSheet(order.active, to);
    return true;
}

int Lienzo::sheetNumber() const
{
    const SheetOrder& order = d->syncOrder();
    return lienzo::sheetNumber(order, d->fixedSheet, order.active);
}

int Lienzo::flipSheetCount() const
{
    return flipCount(d->syncOrder(), d->fixedSheet);
}

QString Lienzo::activeSheetLabel() const
{
    return d->sheetLabel(d->syncOrder().active);
}

void Lienzo::resetSheets(int count)
{
    const Stopwatch time;
    const bool wasRunning = d->running;
    if (wasRunning)
        d->sim->stop();
    const std::vector<uint64_t> before = d->sheets.order().ids;
    d->sheets.reset(d->paper, count);
    for (const uint64_t id : before) // las texturas de las hojas de antes ya no sirven
        d->render->dropSheet(id);
    d->images.clear();
    d->roles = {};
    d->fixedSheet = 0;
    d->fixedReturn = 0;
    d->shownSheet = -1;
    d->releasedTo = 0;
    d->markNumber = 0;
    clearSheetGuides();
    d->sim->setRoles(d->roles);
    d->sim->setFixedSheet(0);
    d->sim->stackReplaced(); // pinta la activa (en blanco)
    d->order = d->sheets.order();
    d->sentRequests = d->sim->sheetState().applied;
    d->applyLayers();
    if (wasRunning)
        d->sim->start();
    qInfo().noquote() << QStringLiteral("Pila armada: %1 hojas en %2 ms").arg(sheetCount()).arg(time.ms(), 0, 'f', 0);
}

bool Lienzo::jumpToEdge(bool last)
{
    const SheetOrder& order = d->syncOrder();
    const int target = flipEdge(order, d->fixedSheet, last);
    if (target < 0 || target == order.active)
        return false;
    activateSheet(target);
    return true;
}

void Lienzo::toggleCircle()
{
    d->circle = !d->circle;
    d->sim->setCircle(d->circle);
    d->applyLayers();
    qInfo() << "Pila en círculo:" << (d->circle ? "sí" : "no");
}

bool Lienzo::circle() const
{
    return d->circle;
}

bool Lienzo::hasFixedSheet() const
{
    return fixedIndex(d->syncOrder(), d->fixedSheet) >= 0;
}

bool Lienzo::onFixedSheet() const
{
    const SheetOrder& order = d->syncOrder();
    return fixedIndex(order, d->fixedSheet) == order.active;
}

void Lienzo::toggleFixedSheet()
{
    d->beforeSheetChange();
    const SheetOrder& order = d->syncOrder();
    if (fixedIndex(order, d->fixedSheet) == order.active) {
        d->fixedSheet = 0;
        qInfo().noquote() << QStringLiteral("Hoja fija: ninguna (vuelve al flip como %1)").arg(d->sheetLabel(order.active));
    } else {
        d->fixedSheet = order.activeId();
        d->fixedReturn = 0;
        qInfo() << "Hoja fija: la activa";
        // Se sigue animando en la vecina (la siguiente, o la anterior en la última).
        int next = flipStep(order, d->fixedSheet, order.active, 1);
        if (next < 0)
            next = flipStep(order, d->fixedSheet, order.active, -1);
        if (next >= 0)
            activateSheet(next);
    }
    d->sim->setFixedSheet(d->fixedSheet);
    d->applyLayers();
}

bool Lienzo::toggleDrawOnFixed()
{
    d->beforeSheetChange();
    const SheetOrder& order = d->syncOrder();
    const int fixed = fixedIndex(order, d->fixedSheet);
    if (fixed < 0) {
        qInfo() << "Hoja fija: no hay (F marca la activa)";
        return false;
    }
    if (fixed == order.active) {
        const int back = d->fixedReturnIndex();
        if (back < 0)
            return false;
        activateSheet(back);
        qInfo().noquote() << QStringLiteral("Hoja fija: vuelve a la %1").arg(d->sheetLabel(back));
    } else {
        d->fixedReturn = order.activeId();
        activateSheet(fixed);
        qInfo() << "Hoja fija: dibujando en ella";
    }
    return true;
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
