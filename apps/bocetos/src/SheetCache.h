#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QThreadPool>

#include <functional>

// HU-137: las hojas del cuaderno abierto, cargadas en segundo plano (RNF-02: pasar de hoja en
// menos de 100 ms). Cada terminada.png es de 600 dpi (7680 × 4800): se lee achicada a `bound`
// (el monitor), y entera solo la que se mira acercada. El visor pide la actual y precarga las
// vecinas; lo que no pide más se suelta.
class SheetCache : public QObject {
public:
    explicit SheetCache(QObject* parent = nullptr);
    ~SheetCache() override; // espera a las cargas que estén en curso

    // Dónde entran las achicadas (px reales); si cambia, se sueltan las que había.
    void setBound(const QSize& bound);
    QSize bound() const { return m_bound; }

    // La imagen si ya está; si no, la empieza a cargar (una vez) y devuelve una vacía.
    QImage get(const QString& path, bool full = false);
    // La imagen si ya está, sin cargarla.
    QImage peek(const QString& path, bool full = false) const { return m_images.value(key(path, full)); }
    // Suelta todo lo que no sea una de `paths` achicada o `full` entera.
    void keepOnly(const QStringList& paths, const QString& full = {});

    // Avisa en el hilo de la app cuando termina una carga (con lo que tardó, en ms).
    std::function<void(const QString& path, bool full, double ms)> onReady;

    // Lee el PNG achicado para que entre en `bound` (vacío: entero); vacía si no se puede.
    static QImage load(const QString& path, const QSize& bound);

private:
    static QString key(const QString& path, bool full) { return (full ? QStringLiteral("F|") : QStringLiteral("S|")) + path; }

    QThreadPool m_pool;
    QSize m_bound{1920, 1080};
    int m_generation = 0; // las cargas de un bound viejo no se guardan
    QHash<QString, QImage> m_images;
    QSet<QString> m_loading;
};
