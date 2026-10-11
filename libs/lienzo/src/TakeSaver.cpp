#include "lienzo/TakeSaver.h"

#include "lienzo/Take.h"

#include <QDebug>

#include <windows.h>

namespace lienzo {

TakeSaver::TakeSaver(QString dir, const drymedia::Paper& paper, SheetImages* images)
    : m_dir(std::move(dir))
    , m_paper(paper)
    , m_images(images)
    , m_thread([this] { run(); })
{
}

TakeSaver::~TakeSaver()
{
    {
        std::lock_guard lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_all();
    m_thread.join();
}

void TakeSaver::saveSheet(uint64_t id, std::vector<uint8_t> state, int width, int height, std::vector<uint32_t> pixels)
{
    {
        std::lock_guard lock(m_mutex);
        m_sheets[id] = {std::move(state), width, height, std::move(pixels)};
    }
    m_wake.notify_all();
}

void TakeSaver::saveIndex(TakeIndex index)
{
    {
        std::lock_guard lock(m_mutex);
        m_index = std::move(index);
    }
    m_wake.notify_all();
}

void TakeSaver::flush()
{
    std::unique_lock lock(m_mutex);
    m_done.wait(lock, [this] { return idle(); });
}

TakeSaver::Stats TakeSaver::stats() const
{
    std::lock_guard lock(m_mutex);
    return m_stats;
}

void TakeSaver::run()
{
    // Comprimir una hoja ocupa un núcleo ~0,1 s: que no le gane al lápiz ni al render.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    std::unique_lock lock(m_mutex);
    while (true) {
        m_wake.wait(lock, [this] { return m_quit || !m_sheets.empty() || m_index; });
        if (m_sheets.empty() && !m_index)
            break; // se pidió terminar y no queda nada
        m_busy = true;
        QString error, what;
        if (!m_sheets.empty()) { // primero las hojas
            auto node = m_sheets.extract(m_sheets.begin());
            const uint64_t id = node.key();
            Pending p = std::move(node.mapped());
            lock.unlock();
            const PackedImage image = !p.pixels.empty() ? PackedImage::pack(p.width, p.height, p.pixels.data())
                                      : m_images        ? m_images->get(id)
                                                        : PackedImage{};
            error = writeSheetFile(m_dir, m_paper, id, p.state, image);
            what = QStringLiteral("hoja-%1").arg(id);
            lock.lock();
            ++m_stats.sheets;
        } else {
            const TakeIndex index = std::move(*m_index);
            m_index.reset();
            lock.unlock();
            error = writeTakeIndex(m_dir, m_paper, index.ids, index.active, {index.degrees, index.zoom, index.panX, index.panY, index.roles});
            what = QStringLiteral("toma.json");
            lock.lock();
            ++m_stats.indexes;
        }
        if (!error.isEmpty()) {
            m_stats.error = QStringLiteral("%1: %2").arg(what, error);
            qWarning().noquote() << QStringLiteral("Toma: no se pudo guardar en %1: %2").arg(m_dir, m_stats.error);
        }
        m_busy = false;
        if (idle())
            m_done.notify_all();
    }
    m_done.notify_all();
}

} // namespace lienzo
