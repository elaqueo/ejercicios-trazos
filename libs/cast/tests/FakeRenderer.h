#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <QUrl>

#include <functional>

class QTcpSocket;

// Un TV de mentira (HU-145) para las pruebas y el smoke: la descripción UPnP en /dmr y el
// AVTransport por SOAP. Anota cada acción y, al aceptar una imagen, la descarga como un TV.
// Así nada se manda al TV de verdad.
class FakeRenderer : public QObject {
public:
    explicit FakeRenderer(QObject* parent = nullptr);

    bool listen(quint16 port = 0); // en 127.0.0.1
    quint16 port() const { return m_server.serverPort(); }
    QUrl location() const;

    int failSets = 0;            // los primeros SetAVTransportURI responden con error
    bool acceptSilently = false; // SetAVTransportURI da error pero queda cargada (la maña de GetMediaInfo)
    bool play701 = false;        // Play responde 701, como el Samsung cuando ya la muestra

    // "SetAVTransportURI <url>", "Play", "Stop", "GetMediaInfo", "GET <url> <código> <bytes> <tipo>".
    QStringList events;
    std::function<void(const QString&)> onEvent;

private:
    void handle(QTcpSocket* socket, const QByteArray& head, const QByteArray& body);
    void event(const QString& text);
    void download(const QString& url);

    QTcpServer m_server;
    QNetworkAccessManager m_net;
    QString m_current; // la URI cargada
};
