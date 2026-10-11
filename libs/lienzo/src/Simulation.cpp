#include "lienzo/Simulation.h"

#include "lienzo/DisplayImage.h"
#include "lienzo/SampleQueue.h"
#include "lienzo/SheetStack.h"
#include "lienzo/Timing.h"
#include "lienzo/Tone.h"

#include <array>

#include <drymedia/History.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <QDebug>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace lienzo {

Simulation::Simulation(drymedia::Paper& paper, SheetStack& sheets, SampleQueue& queue, DisplayImage& image,
                       const SheetMapping& mapping)
    : m_paper(paper)
    , m_sheets(sheets)
    , m_queue(queue)
    , m_image(image)
    , m_mapping(mapping)
    , m_lead(factoryGrades()[kHbIndex].pack())
    , m_eraser(Eraser{}.pack())
{
    m_texture.build(m_paper, m_mapping);
    m_sheets.onLeave = [this](uint64_t id) { retireSheet(id); };
    publishSheetState();
}

Simulation::~Simulation()
{
    stop();
    m_sheets.onLeave = nullptr;
}

void Simulation::start()
{
    m_quit = false;
    m_thread = std::thread([this] { run(); });
}

void Simulation::stop()
{
    m_quit = true;
    m_queue.wake();
    if (m_thread.joinable())
        m_thread.join();
}

void Simulation::wake()
{
    m_queue.wake();
}

void Simulation::setGuides(std::vector<uint32_t> guides)
{
    {
        std::lock_guard lock(m_guidesMutex);
        m_pendingGuides = std::move(guides);
    }
    m_guidesChanged = true;
    wake();
}

bool LightSchedule::due(double view, int percent, double nowMs)
{
    if (view != m_seenView) {
        m_seenView = view;
        m_seenAt = nowMs;
    }
    if (m_percent >= 0 && percent == m_percent && (percent == 0 || view == m_view))
        return false; // nada que cambie la luz (sin textura el giro no importa)
    const bool viewOnly = m_percent >= 0 && percent == m_percent;
    if (viewOnly && nowMs - m_seenAt < kSettleMs)
        return false; // todavía girando
    m_view = view;
    m_percent = percent;
    return true;
}

bool Simulation::updateLight()
{
    double view;
    {
        std::lock_guard rotationLock(m_rotationMutex);
        view = m_rotation.degrees;
    }
    const int percent = m_texturePercent;
    const double nowMs = double(std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now().time_since_epoch()).count()) / 1000.0;
    if (!m_light.due(view, percent, nowMs))
        return false;
    // La luz gira al revés que la vista para quedar fija en la pantalla.
    m_texture.setLight(kLightDegrees - view, percent / 100.0);
    return true;
}

void Simulation::renderAll()
{
    if (m_guidesChanged.exchange(false)) {
        std::lock_guard lock(m_guidesMutex);
        m_guides = std::move(m_pendingGuides);
        m_pendingGuides.clear();
    }
    updateLight();
    std::lock_guard lock(m_image.mutex);
    renderTone(m_paper, m_mapping, 0, 0, m_image.width, m_image.height, m_image.pixels.data(), guides(),
               m_texture.shade());
    m_image.markDirty(0, 0, m_image.width, m_image.height);
    m_image.sheet = m_sheets.order().activeId();
}

void Simulation::pushSheetCommand(SheetCommand command)
{
    command.sample = m_queue.pushed(); // las muestras y los pedidos llegan del mismo hilo
    {
        std::lock_guard lock(m_sheetMutex);
        m_sheetCommands.push_back(command);
    }
    wake();
}

bool Simulation::applySheetCommand(const SheetCommand& c)
{
    const uint64_t tag = m_undoEnabled ? ++m_tag : 0; // las operaciones de la pila se deshacen (HU-87)
    switch (c.kind) {
    case SheetCommand::Add: return m_sheets.add(m_paper, c.a, tag);
    case SheetCommand::Insert: return m_sheets.insertAfterActive(m_paper, tag);
    case SheetCommand::Duplicate: return m_sheets.duplicateActive(m_paper, tag);
    case SheetCommand::Clear: return m_sheets.clearActive(m_paper, tag);
    case SheetCommand::Remove: return m_sheets.remove(m_paper, c.a, tag);
    case SheetCommand::Move: return m_sheets.move(c.a, c.b);
    case SheetCommand::Activate: return m_sheets.activate(m_paper, c.a);
    }
    return false;
}

void Simulation::sheetChanged()
{
    renderAllParallel();
    qInfo().noquote() << QStringLiteral("Hoja activa %1 de %2: imagen en %3 ms")
                             .arg(m_sheets.active() + 1)
                             .arg(m_sheets.count())
                             .arg(m_activationMs.load(), 0, 'f', 1);
}

void Simulation::publishSheetState()
{
    std::lock_guard lock(m_stateMutex);
    m_state.order = m_sheets.order();
    m_state.activeTiles = m_paper.tileCount();
    m_state.applied = m_applied;
}

// HU-83: la imagen de la hoja que sale queda para el render, que la sube a la textura de esa
// hoja (la de la CPU, no la que ya está en la GPU: a la GPU pueden faltarle los últimos
// trazos, porque el render sube la imagen de a partes).
void Simulation::retireSheet(uint64_t id)
{
    RetiredSheet retired;
    retired.sheet = id;
    retired.width = m_mapping.sheetWidth;
    retired.height = m_mapping.sheetHeight;
    retired.pixels.resize(size_t(retired.width) * size_t(retired.height));
    std::lock_guard lock(m_image.mutex);
    for (int y = 0; y < retired.height; ++y)
        std::copy_n(m_image.pixels.data() + size_t(m_mapping.sheetY + y) * size_t(m_image.width) + size_t(m_mapping.sheetX),
                    retired.width, retired.pixels.data() + size_t(y) * size_t(retired.width));
    m_image.retired.push_back(std::move(retired));
}

// Hoja nueva activa (HU-82): la hoja entera de una vez, repartida en filas entre los
// núcleos (en un hilo tarda ~110 ms; spike HU-81). Lo de afuera de la hoja no cambia. El
// render espera el candado lo que dure: es al cambiar de hoja, no mientras se dibuja.
void Simulation::renderAllParallel()
{
    const Stopwatch time;
    const int threads = int(std::max(1u, std::min(8u, std::thread::hardware_concurrency())));
    const int x0 = m_mapping.sheetX, x1 = m_mapping.sheetX + m_mapping.sheetWidth;
    const int top = m_mapping.sheetY, bottom = m_mapping.sheetY + m_mapping.sheetHeight;
    std::lock_guard lock(m_image.mutex);
    const int rows = (bottom - top + threads - 1) / threads;
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        const int y0 = top + t * rows, y1 = std::min(bottom, y0 + rows);
        if (y0 >= y1)
            break;
        pool.emplace_back([this, x0, x1, y0, y1] {
            renderTone(m_paper, m_mapping, x0, y0, x1, y1, m_image.pixels.data(), guides(), m_texture.shade());
        });
    }
    for (std::thread& th : pool)
        th.join();
    m_image.markDirty(x0, top, x1, bottom);
    m_image.sheet = m_sheets.order().activeId();
    m_activationMs = time.ms();
}

// Toda la imagen de a franjas: el candado se suelta entre una y otra para que el render no
// pierda frames (como repaintTiles).
void Simulation::repaintAllInStrips()
{
    constexpr int kStrip = 16;
    for (int y = 0; y < m_image.height; y += kStrip) {
        const int y1 = std::min(y + kStrip, m_image.height);
        std::lock_guard lock(m_image.mutex);
        renderTone(m_paper, m_mapping, 0, y, m_image.width, y1, m_image.pixels.data(), guides(), m_texture.shade());
        m_image.markDirty(0, y, m_image.width, y1);
    }
}

void Simulation::run()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    uint64_t lead = m_lead, eraserParams = m_eraser;
    int grade = m_grade;
    std::array<drymedia::LeadWear, kGradeCount> wears; // desgaste por dureza (HU-62)
    const auto leadMedium = [this](uint64_t packed) { return forProfile(Lead::unpack(packed).medium(), m_profile); };
    const auto eraserMedium = [this](uint64_t packed) { return forProfile(Eraser::unpack(packed).medium(), m_profile); };
    drymedia::Pencil pencil(m_paper, leadMedium(lead));
    pencil.setWear(&wears[size_t(grade)]);
    drymedia::Pencil eraser(m_paper, eraserMedium(eraserParams)); // goma (HU-58)
    drymedia::Pencil stylus(m_paper, drymedia::Medium::stylus());           // punta seca (HU-61)
    // Deshacer y rehacer (HU-53): el lápiz y la goma avisan antes de la primera escritura
    // de cada tile en un trazo, y el historial de la hoja activa guarda cómo estaba (cada hoja
    // tiene el suyo, HU-82).
    const auto observer = [this](int tile, const uint16_t* before, uint8_t planes) {
        m_sheets.history().beforeTileWrite(tile, before, planes);
    };
    if (m_undoEnabled) {
        pencil.setTileObserver(observer);
        eraser.setTileObserver(observer);
        stylus.setTileObserver(observer);
    }
    drymedia::Pencil* active = nullptr; // herramienta del trazo en curso
    const auto beginStroke = [&](drymedia::Pencil& tool, const drymedia::PencilSample& p) {
        if (m_undoEnabled) {
            m_sheets.history().beginStroke(++m_tag);
            m_sheets.clearRedo(); // algo nuevo: lo rehacible de la pila se descarta
        }
        tool.beginStroke(p);
        active = &tool;
    };
    const auto endStroke = [&] {
        if (!active)
            return;
        active->endStroke();
        active = nullptr;
        if (m_undoEnabled)
            m_sheets.history().endStroke(m_paper);
    };
    // Vuelve a pintar el tono de los tiles que cambió el deshacer o el rehacer. El
    // candado de la imagen se toma y se suelta en cada tile (~0,1 ms): tomarlo una vez para
    // todo el trazo lo retenía hasta 8 ms, el render no llegaba a tomar la imagen antes del
    // vsync y perdía frames (y el adelanto adaptativo quedaba alto el resto de la sesión).
    const auto repaintTiles = [&](const std::vector<int>& tiles) {
        for (const int t : tiles) {
            const int tx = t % m_paper.tilesX(), ty = t / m_paper.tilesX();
            int px0, py0, px1, py1;
            if (!m_mapping.pixelsOfCells(tx * drymedia::kTileSize, ty * drymedia::kTileSize,
                                         (tx + 1) * drymedia::kTileSize, (ty + 1) * drymedia::kTileSize, px0, py0, px1, py1))
                continue;
            std::lock_guard lock(m_image.mutex);
            const Stopwatch held;
            renderTone(m_paper, m_mapping, px0, py0, px1, py1, m_image.pixels.data(), guides(), m_texture.shade());
            m_image.markDirty(px0, py0, px1, py1);
            if (m_timings)
                m_timings->simLockHeld.add(held.ms());
        }
    };
    std::vector<tabletinput::PenSample> samples;

    while (!m_quit) {
        m_queue.wait(50);
        if (m_quit)
            break;

        if (m_guidesChanged)
            renderAll(); // guías nuevas: repintar todo una vez
        if (updateLight())
            repaintAllInStrips(); // otra intensidad de textura, o la vista giró bajo la luz
        if (m_clearRequested.exchange(false)) {
            endStroke();
            m_paper.clear();
            m_sheets.history().clear(); // la hoja nueva no se deshace
            renderAll();
        }
        if (lead != m_lead || grade != m_grade) {
            grade = m_grade;
            lead = m_lead;
            endStroke(); // cada trazo con una sola mina: se deshace con la que lo hizo
            pencil.setMedium(leadMedium(lead), &wears[size_t(std::clamp(grade, 0, kGradeCount - 1))]);
        }
        if (m_sharpenRequested.exchange(false))
            pencil.sharpen();
        if (eraserParams != m_eraser) {
            eraserParams = m_eraser;
            endStroke();
            eraser.setMedium(eraserMedium(eraserParams));
        }

        // Las muestras y después los pedidos de la pila: cada pedido sabe cuántas muestras
        // habían llegado cuando se hizo y se atiende justo entre esas y las siguientes, aunque
        // lleguen en el mismo lote (HU-90: soltar la flecha y apoyar el lápiz enseguida).
        const uint64_t firstSample = m_queue.takeAll(samples);
        std::vector<SheetCommand> sheetCommands;
        {
            std::lock_guard lock(m_sheetMutex);
            sheetCommands.swap(m_sheetCommands);
        }
        ViewRotation rotation;
        {
            std::lock_guard rotationLock(m_rotationMutex);
            rotation = m_rotation;
        }
        const Stopwatch batch;
        drymedia::DirtyRect dirty;
        int64_t newest = 0;
        // Lo dibujado hasta acá, a la imagen.
        const auto paint = [&] {
            int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
            const bool any =
                !dirty.empty() && m_mapping.pixelsOfCells(dirty.x0, dirty.y0, dirty.x1, dirty.y1, px0, py0, px1, py1);
            std::lock_guard lock(m_image.mutex);
            const Stopwatch held;
            if (any) {
                renderTone(m_paper, m_mapping, px0, py0, px1, py1, m_image.pixels.data(), guides(), m_texture.shade());
                m_image.markDirty(px0, py0, px1, py1);
            }
            if (newest) {
                m_image.newestSampleUs = newest;
                ++m_image.sampleVersion;
            }
            if (m_timings && any)
                m_timings->simLockHeld.add(held.ms());
            dirty = {};
            newest = 0;
        };
        // Después de cada cosa que cambia la hoja del papel se repinta, antes de la siguiente:
        // la imagen tiene que ser siempre la de la activa.
        const auto applyCommand = [&](const SheetCommand& c) {
            endStroke();
            if (applySheetCommand(c))
                sheetChanged();
            ++m_applied;
        };
        size_t nextCommand = 0;
        for (size_t i = 0; i < samples.size(); ++i) {
            // Los pedidos hechos antes de esta muestra, con lo anterior ya en la imagen (así la
            // hoja que sale deja su imagen con todos sus trazos, HU-83).
            for (; nextCommand < sheetCommands.size() && sheetCommands[nextCommand].sample <= firstSample + i;
                 ++nextCommand) {
                paint();
                applyCommand(sheetCommands[nextCommand]);
            }
            const tabletinput::PenSample& s = samples[i];
            // Las muestras ya llegan en coordenadas del cliente (CanvasWindow resta su esquina);
            // el mapeo las pasa a celdas de la hoja (a escala de la tableta, HU-52).
            double ix = s.x, iy = s.y;
            rotation.toImage(ix, iy); // vista rotada: de vuelta a la hoja sin rotar
            // La inclinación también gira con la vista.
            const float azimuth = float(std::fmod(double(s.azimuth) - rotation.degrees + 720.0, 360.0));
            const drymedia::PencilSample p{m_mapping.cellX(ix), m_mapping.cellY(iy), s.pressure, azimuth,
                                           penAltitude(s.altitude, m_tilt)};
            if (m_recording) {
                const Lead l = Lead::unpack(lead);
                std::fprintf(m_recording, "%lld,%.4f,%.4f,%.3f,%.3f,%.4f,%.2f,%.2f,%d,%d,%d,%d,%d\n",
                             static_cast<long long>(s.timeUs), s.x, s.y, p.x, p.y, s.pressure, s.azimuth, s.altitude,
                             int(s.inContact), int(s.eraser), l.softness, l.diameter, l.ceiling);
            }
            m_erasing = s.eraser;
            drymedia::Pencil& tool = s.eraser ? eraser : m_stylus ? stylus : pencil;
            if (s.inContact) {
                newest = s.timeUs;
                if (active != &tool) {
                    endStroke(); // dar vuelta el lápiz sin levantarlo: otro trazo
                    beginStroke(tool, p);
                } else {
                    dirty.unite(tool.strokeTo(p));
                }
            } else {
                endStroke();
            }
        }
        const double pencilMs = batch.ms();
        m_wearPercent = pencil.wear().percent();
        paint();
        if (m_timings && !samples.empty()) {
            m_timings->simPencil.add(pencilMs);
            m_timings->simBatch.add(batch.ms());
        }

        // Al final del lote: deshacer y rehacer (si llegan en medio de un trazo, primero se
        // cierra el trazo y deshacer lo borra entero) y los pedidos que llegaron después de la
        // última muestra.
        const int undos = m_undoRequests.exchange(0), redos = m_redoRequests.exchange(0);
        if (undos || redos) {
            const Stopwatch undoTime;
            const auto apply = [&](const SheetStack::UndoResult& r) {
                if (r.sheetChanged)
                    sheetChanged();
                else
                    repaintTiles(r.tiles);
            };
            for (int n = undos; n > 0; --n) {
                endStroke();
                apply(m_sheets.undo(m_paper));
            }
            for (int n = redos; n > 0; --n) {
                endStroke();
                apply(m_sheets.redo(m_paper));
            }
            m_applied += uint64_t(undos + redos);
            if (m_timings)
                m_timings->simUndo.add(undoTime.ms());
        }
        for (; nextCommand < sheetCommands.size(); ++nextCommand)
            applyCommand(sheetCommands[nextCommand]);
        publishSheetState();
        if (m_applied != m_notified) {
            m_notified = m_applied;
            if (m_onSheetState)
                m_onSheetState();
        }
    }
}

} // namespace lienzo
