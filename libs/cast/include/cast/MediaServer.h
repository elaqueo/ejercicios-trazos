#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QTcpServer>
#include <QUrl>

#include <functional>

class QTcpSocket;

namespace cast {

// El servidor HTTP de donde el TV descarga la imagen (HU-145): GET y HEAD de las últimas
// imágenes publicadas, con las cabeceras DLNA; lo demás, 404. Cada imagen tiene una ruta
// propia (/hoja/<token>/<n>.jpg), así el TV no muestra una vieja que tenga guardada. En un
// puerto fijo, para que la regla del firewall de Windows no cambie.
class MediaServer : public QObject {
public:
    explicit MediaServer(QObject* parent = nullptr);

    // En todas las interfaces IPv4, o solo en `address` (127.0.0.1 para el TV de mentira: así el
    // firewall de Windows no pregunta). Si ya escucha en otra dirección, se muda.
    bool listen(quint16 port, const QHostAddress& address = QHostAddress(QHostAddress::AnyIPv4));
    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }
    QString errorString() const { return m_server.errorString(); }

    // Publica un JPEG y devuelve su ruta; quedan las últimas kKept.
    QString publish(const QByteArray& jpeg);
    QUrl urlFor(const QString& path, const QHostAddress& local) const;

    // Cada pedido que llega: el método, la ruta, el código y los bytes del cuerpo.
    std::function<void(const QString& method, const QString& path, int status, qint64 bytes)> onRequest;

    static constexpr int kKept = 4;

private:
    void answer(QTcpSocket* socket, const QByteArray& request);

    QTcpServer m_server;
    QString m_token;
    int m_count = 0;
    QList<QPair<QString, QByteArray>> m_files;
};

} // namespace cast
