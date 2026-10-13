#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

#include <functional>

// HU-143: las miniaturas de la biblioteca, en segundo plano. Cada una se guarda como miniatura.png
// (480 × 300) junto a su hoja: se arma una vez (leer una hoja de 7680 × 4800 tarda ~260 ms) y se
// rehace si la imagen es más nueva (el borrador de Krita que cambió); después se lee en
// milisegundos. Las leídas quedan en memoria, por la imagen y su fecha.
class ThumbCache : public QObject {
public:
    explicit ThumbCache(QObject* parent = nullptr);
    ~ThumbCache() override; // espera a las que estén en curso

    // La miniatura de `source` (guardada en `file`) si ya está en memoria; si no, la empieza a
    // leer, o a armar, y devuelve una vacía.
    QImage get(const QString& source, const QString& file);
    bool loading(const QString& source) const;

    // Avisa en el hilo de la app cuando una está lista (con lo que tardó y si se armó).
    std::function<void(const QString& source, double ms, bool built)> onReady;

private:
    static QString key(const QString& source);

    QThreadPool m_pool;
    QHash<QString, QImage> m_images;
    QSet<QString> m_loading;
};
