#pragma once

#include "cast/Dlna.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>
#include <optional>

namespace cast {

// Le pide a un TV que muestre una imagen (HU-145): SetAVTransportURI con los datos de una foto y
// Play. Las mañas del Samsung, de pdfcast: si SetAVTransportURI da error, se pregunta qué tiene
// cargado (GetMediaInfo; a veces la acepta y la respuesta no llega) y, si no es la nueva, Stop y
// se reintenta una vez; el error 701 de Play ("transition not available") es que ya la muestra.
// Una imagen por vez: si se pide otra mientras manda, se manda solo la última al terminar.
class AvTransport : public QObject {
public:
    explicit AvTransport(QObject* parent = nullptr);

    void setRenderer(const Renderer& renderer) { m_renderer = renderer; }
    const Renderer& renderer() const { return m_renderer; }

    void show(const QUrl& url, const QString& title);
    void stop(); // Stop, sin esperar la respuesta; descarta lo pendiente
    bool busy() const { return m_busy; }

    // La imagen ya está en el TV (con lo que tardó desde que se pidió).
    std::function<void(const QUrl& url, double ms)> onShown;
    std::function<void(const QString& error)> onError;

private:
    struct Reply {
        int status = 0;
        QByteArray body;
        QString error; // de la red, si no hubo respuesta
        bool ok() const { return status == 200; }
        QString describe() const;
    };
    void call(const QString& action, const SoapArgs& args, int timeoutMs, std::function<void(const Reply&)> done);
    void next();
    void set(bool retried);
    void play();
    void finish();

    QNetworkAccessManager m_net;
    Renderer m_renderer;
    bool m_busy = false;
    std::optional<std::pair<QUrl, QString>> m_pending;
    QUrl m_url;
    QString m_title;
    QElapsedTimer m_time;
    int m_generation = 0; // stop() descarta las respuestas en camino
};

} // namespace cast
