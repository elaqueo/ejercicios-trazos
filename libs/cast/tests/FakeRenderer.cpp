#include "FakeRenderer.h"

#include <cast/Dlna.h>

#include <QNetworkReply>
#include <QTcpSocket>

#include <memory>

namespace {

const char* const kDescription =
    "<?xml version=\"1.0\"?>"
    "<root xmlns=\"urn:schemas-upnp-org:device-1-0\"><specVersion><major>1</major><minor>0</minor></specVersion>"
    "<device><deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>"
    "<friendlyName>TV de prueba</friendlyName>"
    "<serviceList>"
    "<service><serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>"
    "<controlURL>/upnp/control/ConnectionManager1</controlURL></service>"
    "<service><serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>"
    "<controlURL>/upnp/control/AVTransport1</controlURL></service>"
    "</serviceList></device></root>";

QByteArray soapResponse(const QString& action, const QString& inner = {})
{
    return QStringLiteral("<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
                          "<u:%1Response xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">%2</u:%1Response>"
                          "</s:Body></s:Envelope>")
        .arg(action, inner)
        .toUtf8();
}

QByteArray upnpError(int code, const QString& text)
{
    return QStringLiteral("<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
                          "<s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail>"
                          "<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\"><errorCode>%1</errorCode>"
                          "<errorDescription>%2</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>")
        .arg(code)
        .arg(text)
        .toUtf8();
}

void reply(QTcpSocket* socket, int status, const QByteArray& body, const QByteArray& type = "text/xml; charset=\"utf-8\"")
{
    const QByteArray reason = status == 200 ? "OK" : status == 404 ? "Not Found" : "Internal Server Error";
    socket->write("HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\nContent-Type: " + type +
                  "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
}

} // namespace

FakeRenderer::FakeRenderer(QObject* parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                const qsizetype end = buffer->indexOf("\r\n\r\n");
                if (end < 0)
                    return;
                const QByteArray head = buffer->left(end);
                qsizetype length = 0;
                for (const QByteArray& line : head.split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toLongLong();
                if (buffer->size() < end + 4 + length)
                    return; // falta el cuerpo
                const QByteArray body = buffer->mid(end + 4, length);
                buffer->clear();
                handle(socket, head, body);
            });
        }
    });
}

bool FakeRenderer::listen(quint16 port)
{
    return m_server.listen(QHostAddress::LocalHost, port);
}

QUrl FakeRenderer::location() const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/dmr").arg(port()));
}

void FakeRenderer::event(const QString& text)
{
    events.append(text);
    if (onEvent)
        onEvent(text);
}

void FakeRenderer::handle(QTcpSocket* socket, const QByteArray& head, const QByteArray& body)
{
    const QList<QByteArray> first = head.left(head.indexOf("\r\n")).split(' ');
    const QByteArray method = first.value(0), path = first.value(1);
    if (method == "GET" && path == "/dmr") {
        reply(socket, 200, kDescription);
        return;
    }
    if (method != "POST" || path != "/upnp/control/AVTransport1") {
        reply(socket, 404, {});
        return;
    }
    QString action;
    for (const QByteArray& line : head.split('\n'))
        if (line.toLower().startsWith("soapaction:")) {
            const QString value = QString::fromLatin1(line.mid(11)).trimmed().remove(QLatin1Char('"'));
            action = value.mid(value.indexOf(QLatin1Char('#')) + 1);
        }
    if (action == QLatin1String("SetAVTransportURI")) {
        const QString uri = cast::xmlValue(body, QStringLiteral("CurrentURI"));
        event(QStringLiteral("SetAVTransportURI %1").arg(uri));
        if (failSets > 0) {
            --failSets;
            if (acceptSilently) {
                m_current = uri;
                download(uri);
            }
            reply(socket, 500, upnpError(714, QStringLiteral("Illegal MIME-type")));
            return;
        }
        m_current = uri;
        reply(socket, 200, soapResponse(action));
        download(uri);
    } else if (action == QLatin1String("Play")) {
        event(QStringLiteral("Play"));
        if (play701)
            reply(socket, 500, upnpError(701, QStringLiteral("Transition not available")));
        else
            reply(socket, 200, soapResponse(action));
    } else if (action == QLatin1String("GetMediaInfo")) {
        event(QStringLiteral("GetMediaInfo"));
        reply(socket, 200, soapResponse(action, QStringLiteral("<CurrentURI>%1</CurrentURI>").arg(m_current.toHtmlEscaped())));
    } else {
        event(action);
        reply(socket, 200, soapResponse(action));
    }
}

void FakeRenderer::download(const QString& url)
{
    QNetworkReply* r = m_net.get(QNetworkRequest(QUrl(url)));
    connect(r, &QNetworkReply::finished, this, [this, r, url] {
        r->deleteLater();
        const QByteArray data = r->readAll();
        event(QStringLiteral("GET %1 %2 %3 %4")
                  .arg(url)
                  .arg(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                  .arg(data.size())
                  .arg(r->header(QNetworkRequest::ContentTypeHeader).toString()));
    });
}
