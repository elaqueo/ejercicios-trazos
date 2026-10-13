#include "SheetCache.h"

#include <QElapsedTimer>
#include <QImageReader>
#include <QMetaObject>

SheetCache::SheetCache(QObject* parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2); // la siguiente y la anterior a la vez
}

SheetCache::~SheetCache()
{
    m_pool.clear();
    m_pool.waitForDone();
}

void SheetCache::setBound(const QSize& bound)
{
    if (bound == m_bound || bound.isEmpty())
        return;
    m_bound = bound;
    ++m_generation;
    m_images.clear();
    m_loading.clear();
}

QImage SheetCache::load(const QString& path, const QSize& bound)
{
    QImageReader reader(path);
    const QSize size = reader.size();
    if (!bound.isEmpty() && size.isValid() && (size.width() > bound.width() || size.height() > bound.height()))
        reader.setScaledSize(size.scaled(bound, Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (image.isNull())
        qWarning().noquote() << QStringLiteral("Cuaderno: no se pudo leer %1 (%2)").arg(path, reader.errorString());
    return image;
}

QImage SheetCache::get(const QString& path, bool full)
{
    const QString k = key(path, full);
    if (const auto it = m_images.constFind(k); it != m_images.cend())
        return *it;
    if (path.isEmpty() || m_loading.contains(k))
        return {};
    m_loading.insert(k);
    const QSize bound = full ? QSize() : m_bound;
    const int generation = m_generation;
    m_pool.start([this, path, full, bound, generation, k] {
        QElapsedTimer time;
        time.start();
        QImage image = load(path, bound);
        const double ms = time.nsecsElapsed() / 1e6;
        // De vuelta en el hilo de la app; si el caché se destruye antes, el evento se descarta.
        QMetaObject::invokeMethod(
            this,
            [this, path, full, generation, k, ms, image = std::move(image)] {
                if (generation != m_generation || !m_loading.remove(k))
                    return; // de un bound viejo, o ya no se quiere
                m_images.insert(k, image);
                if (onReady)
                    onReady(path, full, ms);
            },
            Qt::QueuedConnection);
    });
    return {};
}

void SheetCache::keepOnly(const QStringList& paths, const QString& full)
{
    QSet<QString> wanted;
    for (const QString& path : paths)
        wanted.insert(key(path, false));
    if (!full.isEmpty())
        wanted.insert(key(full, true));
    for (auto it = m_images.begin(); it != m_images.end();)
        it = wanted.contains(it.key()) ? std::next(it) : m_images.erase(it);
    // Las que están cargando y ya no se quieren no se guardan al llegar.
    for (auto it = m_loading.begin(); it != m_loading.end();)
        it = wanted.contains(*it) ? std::next(it) : m_loading.erase(it);
}
