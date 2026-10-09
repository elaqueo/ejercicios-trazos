#include "Simulation.h"

#include "DisplayImage.h"
#include "SampleQueue.h"
#include "Tone.h"

#include <drymedia/Paper.h>
#include <drymedia/Pencil.h>

#include <algorithm>
#include <cmath>

namespace cartuchera {

Simulation::Simulation(drymedia::Paper& paper, SampleQueue& queue, DisplayImage& image, const SheetMapping& mapping)
    : m_paper(paper)
    , m_queue(queue)
    , m_image(image)
    , m_mapping(mapping)
    , m_softness(drymedia::Medium::hb().softness)
    , m_diameter(int(std::lround(drymedia::Medium::hb().leadDiameterMm * 100)))
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

void Simulation::renderAll()
{
    std::lock_guard lock(m_image.mutex);
    renderTone(m_paper, m_mapping, 0, 0, m_image.width, m_image.height, m_image.pixels.data());
    m_image.markDirty(0, 0, m_image.width, m_image.height);
}

void Simulation::run()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    int diameter = m_diameter;
    drymedia::Medium medium = drymedia::Medium::hb().withLeadDiameter(diameter / 100.0);
    medium.softness = uint16_t(m_softness.load());
    drymedia::Pencil pencil(m_paper, medium);
    std::vector<tabletinput::PenSample> samples;

    while (!m_quit) {
        m_queue.wait(50);
        if (m_quit)
            break;

        if (m_clearRequested.exchange(false)) {
            pencil.endStroke();
            m_paper.clear();
            renderAll();
        }
        if (medium.softness != m_softness || diameter != m_diameter) {
            diameter = m_diameter;
            medium = drymedia::Medium::hb().withLeadDiameter(diameter / 100.0);
            medium.softness = uint16_t(std::clamp(m_softness.load(), 1, 255));
            pencil.setMedium(medium);
        }

        m_queue.takeAll(samples);
        if (samples.empty())
            continue;

        drymedia::DirtyRect dirty;
        int64_t newest = 0;
        for (const tabletinput::PenSample& s : samples) {
            // Las muestras ya llegan en coordenadas del cliente (CanvasWindow resta su esquina);
            // el mapeo las pasa a celdas de la hoja (a escala de la tableta, HU-52).
            const drymedia::PencilSample p{m_mapping.cellX(s.x), m_mapping.cellY(s.y), s.pressure, s.azimuth,
                                          s.altitude};
            if (s.eraser) {
                if (pencil.inStroke())
                    pencil.endStroke(); // la goma llega en la Fase 2
                continue;
            }
            if (s.inContact) {
                newest = s.timeUs;
                if (!pencil.inStroke())
                    pencil.beginStroke(p);
                else
                    dirty.unite(pencil.strokeTo(p));
            } else if (pencil.inStroke()) {
                pencil.endStroke();
            }
        }

        int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
        const bool any = !dirty.empty() && m_mapping.pixelsOfCells(dirty.x0, dirty.y0, dirty.x1, dirty.y1, px0, py0, px1, py1);
        std::lock_guard lock(m_image.mutex);
        if (any) {
            renderTone(m_paper, m_mapping, px0, py0, px1, py1, m_image.pixels.data());
            m_image.markDirty(px0, py0, px1, py1);
        }
        if (newest) {
            m_image.newestSampleUs = newest;
            ++m_image.sampleVersion;
        }
    }
}

} // namespace cartuchera
