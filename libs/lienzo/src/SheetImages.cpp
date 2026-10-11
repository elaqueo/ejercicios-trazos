#include "lienzo/SheetImages.h"

#include "Zstd.h"

namespace lienzo {

PackedImage PackedImage::pack(int width, int height, const uint32_t* pixels)
{
    PackedImage image;
    image.width = width;
    image.height = height;
    image.bytes = zstd::compress(pixels, size_t(width) * size_t(height) * sizeof(uint32_t));
    return image;
}

bool PackedImage::unpack(std::vector<uint32_t>& pixels) const
{
    if (empty() || width <= 0 || height <= 0)
        return false;
    pixels.resize(size_t(width) * size_t(height));
    return zstd::decompress(bytes.data(), bytes.size(), pixels.data(), pixels.size() * sizeof(uint32_t));
}

SheetImages::SheetImages()
    : m_thread([this] { run(); })
{
}

SheetImages::~SheetImages()
{
    {
        std::lock_guard lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_all();
    m_thread.join();
}

void SheetImages::put(uint64_t sheet, int width, int height, std::vector<uint32_t> pixels)
{
    {
        std::lock_guard lock(m_mutex);
        m_pending.push_back({sheet, width, height, std::move(pixels)});
    }
    m_wake.notify_all();
}

void SheetImages::putPacked(uint64_t sheet, PackedImage image)
{
    std::lock_guard lock(m_mutex);
    m_images[sheet] = std::move(image);
}

PackedImage SheetImages::get(uint64_t sheet)
{
    std::unique_lock lock(m_mutex);
    m_done.wait(lock, [this] { return m_pending.empty() && !m_busy; });
    const auto it = m_images.find(sheet);
    return it == m_images.end() ? PackedImage{} : it->second;
}

void SheetImages::clear()
{
    std::unique_lock lock(m_mutex);
    m_done.wait(lock, [this] { return m_pending.empty() && !m_busy; });
    m_images.clear();
}

void SheetImages::run()
{
    std::unique_lock lock(m_mutex);
    while (true) {
        m_wake.wait(lock, [this] { return m_quit || !m_pending.empty(); });
        if (m_quit)
            break;
        Raw raw = std::move(m_pending.front());
        m_pending.pop_front();
        m_busy = true;
        lock.unlock();
        PackedImage image = PackedImage::pack(raw.width, raw.height, raw.pixels.data());
        lock.lock();
        m_images[raw.sheet] = std::move(image);
        m_busy = false;
        if (m_pending.empty())
            m_done.notify_all();
    }
    m_busy = false;
    m_done.notify_all();
}

} // namespace lienzo
