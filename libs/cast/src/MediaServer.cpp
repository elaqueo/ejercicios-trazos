#include "cast/MediaServer.h"

#include "cast/Dlna.h"

#include <QRandomGenerator>
#include <QTcpSocket>

#include <memory>

namespace cast {

MediaServer::MediaServer(QObject* parent)
    : QObject(parent)
    , m_token(QString::number(QRandomGenerator::global()->generate(), 16))
{
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                if (buffer->size() > 16 * 1024) { // no es un pedido de un TV
                    socket->abort();
                    return;
                }
                if (buffer->contains("\r\n\r\n")) {
                    const QByteArray request = *buffer;
                    buffer->clear();
                    answer(socket, request);
                }
            });
        }
    });
}

bool MediaServer::listen(quint16 port, const QHostAddress& address)
{
    if (m_server.isListening() && m_server.serverAddress() == address)
        return true;
    m_server.close();
    return m_server.listen(address, port);
}

QString MediaServer::publish(const QByteArray& jpeg)
{
    const QString path = QStringLiteral("/hoja/%1/%2.jpg").arg(m_token).arg(++m_count);
    m_files.append({path, jpeg});
    while (m_files.size() > kKept)
        m_files.removeFirst();
    return path;
}

QUrl MediaServer::urlFor(const QString& path, const QHostAddress& local) const
{
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(local.toString());
    url.setPort(port());
    url.setPath(path);
    return url;
}

void MediaServer::answer(QTcpSocket* socket, const QByteArray& request)
{
    const QList<QByteArray> first = request.left(request.indexOf("\r\n")).split(' ');
    const QString method = first.size() > 0 ? QString::fromLatin1(first[0]) : QString();
    QString path = first.size() > 1 ? QString::fromLatin1(first[1]) : QString();
    if (const qsizetype query = path.indexOf(QLatin1Char('?')); query >= 0)
        path.truncate(query);
    const QByteArray* body = nullptr;
    for (const auto& [name, data] : m_files)
        if (name == path)
            body = &data;
    QByteArray head;
    int status = 404;
    if ((method == QLatin1String("GET") || method == QLatin1String("HEAD")) && body) {
        status = 200;
        head = "HTTP/1.1 200 OK\r\n"
               "Content-Type: image/jpeg\r\n"
               "Content-Length: " + QByteArray::number(body->size()) + "\r\n"
               "transferMode.dlna.org: Interactive\r\n"
               "contentFeatures.dlna.org: " + QByteArray(kDlnaFlags) + "\r\n"
               "Cache-Control: no-cache\r\n"
               "Connection: close\r\n\r\n";
    } else {
        head = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }
    socket->write(head);
    qint64 bytes = 0;
    if (status == 200 && method == QLatin1String("GET")) {
        socket->write(*body);
        bytes = body->size();
    }
    socket->disconnectFromHost(); // después de mandar lo pendiente
    if (onRequest)
        onRequest(method, path, status, bytes);
}

} // namespace cast
