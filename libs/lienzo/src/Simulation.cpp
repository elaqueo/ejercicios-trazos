#include "lienzo/Simulation.h"

#include "lienzo/DisplayImage.h"
#include "lienzo/SampleQueue.h"
#include "lienzo/Timing.h"
#include "lienzo/Tone.h"

#include <array>

#include <drymedia/History.h>
#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <algorithm>
#include <cmath>

namespace lienzo {

Simulation::Simulation(drymedia::Paper& paper, SampleQueue& queue, DisplayImage& image, const SheetMapping& mapping)
    : m_paper(paper)
    , m_queue(queue)
    , m_image(image)
    , m_mapping(mapping)
    , m_lead(factoryGrades()[kHbIndex].pack())
    , m_eraser(Eraser{}.pack())
{
}

Simulation::~Simulation()
{
    stop();
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

void Simulation::renderAll()
{
    if (m_guidesChanged.exchange(false)) {
        std::lock_guard lock(m_guidesMutex);
        m_guides = std::move(m_pendingGuides);
        m_pendingGuides.clear();
    }
    std::lock_guard lock(m_image.mutex);
    renderTone(m_paper, m_mapping, 0, 0, m_image.width, m_image.height, m_image.pixels.data(), guides());
    m_image.markDirty(0, 0, m_image.width, m_image.height);
}

void Simulation::run()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    uint64_t lead = m_lead, eraserParams = m_eraser;
    int grade = m_grade;
    std::array<drymedia::LeadWear, kGradeCount> wears; // desgaste por dureza (HU-62)
    drymedia::Pencil pencil(m_paper, Lead::unpack(lead).medium());
    pencil.setWear(&wears[size_t(grade)]);
    drymedia::Pencil eraser(m_paper, Eraser::unpack(eraserParams).medium()); // goma (HU-58)
    drymedia::Pencil stylus(m_paper, drymedia::Medium::stylus());           // punta seca (HU-61)
    // Deshacer y rehacer (HU-53): el lápiz y la goma avisan antes de la primera escritura
    // de cada tile en un trazo, y el historial guarda cómo estaba.
    drymedia::History history(kUndoLimit);
    const auto observer = [&history](int tile, const uint16_t* before) { history.beforeTileWrite(tile, before); };
    if (m_undoEnabled) {
        pencil.setTileObserver(observer);
        eraser.setTileObserver(observer);
        stylus.setTileObserver(observer);
    }
    drymedia::Pencil* active = nullptr; // herramienta del trazo en curso
    const auto beginStroke = [&](drymedia::Pencil& tool, const drymedia::PencilSample& p) {
        if (m_undoEnabled)
            history.beginStroke();
        tool.beginStroke(p);
        active = &tool;
    };
    const auto endStroke = [&] {
        if (!active)
            return;
        active->endStroke();
        active = nullptr;
        if (m_undoEnabled)
            history.endStroke(m_paper);
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
            renderTone(m_paper, m_mapping, px0, py0, px1, py1, m_image.pixels.data(), guides());
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
        if (m_clearRequested.exchange(false)) {
            endStroke();
            m_paper.clear();
            history.clear(); // la hoja nueva no se deshace
            renderAll();
        }
        if (lead != m_lead || grade != m_grade) {
            grade = m_grade;
            lead = m_lead;
            endStroke(); // cada trazo con una sola mina: se deshace con la que lo hizo
            pencil.setMedium(Lead::unpack(lead).medium(), &wears[size_t(std::clamp(grade, 0, kGradeCount - 1))]);
        }
        if (m_sharpenRequested.exchange(false))
            pencil.sharpen();
        if (eraserParams != m_eraser) {
            eraserParams = m_eraser;
            endStroke();
            eraser.setMedium(Eraser::unpack(eraserParams).medium());
        }

        m_queue.takeAll(samples);
        ViewRotation rotation;
        {
            std::lock_guard rotationLock(m_rotationMutex);
            rotation = m_rotation;
        }
        const Stopwatch batch;
        drymedia::DirtyRect dirty;
        int64_t newest = 0;
        for (const tabletinput::PenSample& s : samples) {
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

        // Deshacer y rehacer después de las muestras del lote: si llegan en medio de un
        // trazo, primero se cierra el trazo (y deshacer lo borra entero).
        const int undos = m_undoRequests.exchange(0), redos = m_redoRequests.exchange(0);
        if (undos || redos) {
            const Stopwatch undoTime;
            for (int n = undos; n > 0; --n) {
                endStroke();
                repaintTiles(history.undo(m_paper));
            }
            for (int n = redos; n > 0; --n) {
                endStroke();
                repaintTiles(history.redo(m_paper));
            }
            if (m_timings)
                m_timings->simUndo.add(undoTime.ms());
        }

        int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
        const bool any = !dirty.empty() && m_mapping.pixelsOfCells(dirty.x0, dirty.y0, dirty.x1, dirty.y1, px0, py0, px1, py1);
        {
            std::lock_guard lock(m_image.mutex);
            const Stopwatch held;
            if (any) {
                renderTone(m_paper, m_mapping, px0, py0, px1, py1, m_image.pixels.data(), guides());
                m_image.markDirty(px0, py0, px1, py1);
            }
            if (newest) {
                m_image.newestSampleUs = newest;
                ++m_image.sampleVersion;
            }
            if (m_timings && any)
                m_timings->simLockHeld.add(held.ms());
        }
        if (m_timings && !samples.empty()) {
            m_timings->simPencil.add(pencilMs);
            m_timings->simBatch.add(batch.ms());
        }
    }
}

} // namespace lienzo
