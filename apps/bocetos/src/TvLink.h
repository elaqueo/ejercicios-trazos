#pragma once

#include "BocetosCore.h"

#include <appkit/Config.h>

#include <cast/AvTransport.h>
#include <cast/Discovery.h>
#include <cast/MediaServer.h>

#include <QColor>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QThreadPool>
#include <QUrl>

#include <functional>

// HU-145: la hoja del cuaderno abierto en el TV. T conecta (al último TV, o busca; con varios,
// se elige de una lista) y desde ahí cada hoja que se mira va también al TV, como JPEG servido
// por el servidor de cast en el puerto 8766; T otra vez, o volver a la biblioteca, corta.
class TvLink : public QObject {
public:
    static constexpr quint16 kPort = 8766; // fijo, por el firewall (pdfcast usa 8765)

    explicit TvLink(appkit::Config& config, QObject* parent = nullptr);

    // --tv: ese TV, sin buscar ni recordarlo (el TV de mentira de la prueba de la app real).
    void setForced(const QUrl& location) { m_forced = location; }

    enum class State { Off, Searching, On };
    State state() const { return m_state; }
    void toggle();
    void disconnect();
    void choose(const cast::Renderer& renderer); // de la lista

    // La hoja que se mira (se guarda aunque no esté conectado: es la que va al conectar).
    void show(const bocetos::Page& page, const QColor& paper, const QString& title);

    // Para la barra: el TV y qué hace ("mostrando la hoja 7").
    QString name() const { return m_renderer.name; }
    QString status() const { return m_status; }

    std::function<void()> onChanged; // repintar la barra
    std::function<void(const QString& title, const QString& detail)> onWarn;
    std::function<void(const QString& title, const QString& detail)> onBusy;
    std::function<void()> onIdle;
    std::function<void(const QList<cast::Renderer>&)> onChoose; // más de un TV

private:
    void found(const QList<cast::Renderer>& renderers, bool searched);
    void connectTo(const cast::Renderer& renderer);
    void send();
    void setStatus(const QString& status);

    appkit::Config& m_config;
    cast::Discovery m_discovery;
    cast::MediaServer m_server;
    cast::AvTransport m_transport;
    QThreadPool m_pool; // arma el JPEG fuera del hilo de la app
    QUrl m_forced;
    State m_state = State::Off;
    cast::Renderer m_renderer;
    QHostAddress m_local;
    QString m_status;
    struct Want {
        bocetos::Page page;
        QColor paper;
        QString title;
    } m_want;
    bool m_hasWant = false;
    int m_wantGeneration = 0;
    QHash<QString, int> m_sheetOfUrl; // para decir qué hoja quedó en el TV
};
