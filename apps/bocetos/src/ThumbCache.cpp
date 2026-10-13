#include "ThumbCache.h"

#include "BocetosCore.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>

namespace {
constexpr int kKeep = 90; // unas 30 tapas de 3 hojas (~50 MB); más, y se suelta todo
}

ThumbCache::ThumbCache(QObject* parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2);
}

ThumbCache::~ThumbCache()
{
    m_pool.clear();
    m_pool.waitForDone();
}

QString ThumbCache::key(const QString& source)
{
    return source + QLatin1Char('|') + QString::number(QFileInfo(source).lastModified().toMSecsSinceEpoch());
}

bool ThumbCache::loading(const QString& source) const
{
    return m_loading.contains(key(source));
}

QImage ThumbCache::get(const QString& source, const QString& file)
{
    const QString k = key(source);
    if (const auto it = m_images.constFind(k); it != m_images.cend())
        return *it;
    if (source.isEmpty() || m_loading.contains(k))
        return {};
    m_loading.insert(k);
    m_pool.start([this, source, file, k] {
        QElapsedTimer time;
        time.start();
        const bool build = !bocetos::thumbFresh(source, file);
        QImage image = build ? bocetos::writeThumb(source, file) : QImage(file);
        const double ms = time.nsecsElapsed() / 1e6;
        QMetaObject::invokeMethod(
            this,
            [this, source, k, ms, build, image = std::move(image)] {
                if (!m_loading.remove(k))
                    return;
                if (m_images.size() >= kKeep)
                    m_images.clear();
                m_images.insert(k, image);
                if (onReady)
                    onReady(source, ms, build);
            },
            Qt::QueuedConnection);
    });
    return {};
}
