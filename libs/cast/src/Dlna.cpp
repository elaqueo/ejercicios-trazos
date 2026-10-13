#include "cast/Dlna.h"

#include <QNetworkInterface>
#include <QStringList>
#include <QXmlStreamReader>

namespace cast {

QByteArray searchMessage(int mx)
{
    return QStringLiteral("M-SEARCH * HTTP/1.1\r\n"
                          "HOST: %1:%2\r\n"
                          "MAN: \"ssdp:discover\"\r\n"
                          "MX: %3\r\n"
                          "ST: %4\r\n"
                          "\r\n")
        .arg(QLatin1String(kSsdpGroup))
        .arg(kSsdpPort)
        .arg(mx)
        .arg(QLatin1String(kAvTransport))
        .toLatin1();
}

QUrl locationOf(const QByteArray& response)
{
    for (const QByteArray& line : response.split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon > 0 && line.left(colon).trimmed().toLower() == "location")
            return QUrl(QString::fromLatin1(line.mid(colon + 1).trimmed()));
    }
    return {};
}

std::optional<Renderer> parseDescription(const QByteArray& xml, const QUrl& location)
{
    QXmlStreamReader reader(xml);
    QUrl base = location;
    QStringList devices; // el friendlyName de cada <device> abierto
    QString serviceType, controlUrl;
    bool inService = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const QStringView name = reader.name();
            if (name == u"URLBase")
                base = QUrl(reader.readElementText().trimmed());
            else if (name == u"device")
                devices.append(QString());
            else if (name == u"friendlyName" && !devices.isEmpty() && devices.last().isEmpty())
                devices.last() = reader.readElementText().trimmed();
            else if (name == u"service") {
                inService = true;
                serviceType.clear();
                controlUrl.clear();
            } else if (inService && name == u"serviceType")
                serviceType = reader.readElementText().trimmed();
            else if (inService && name == u"controlURL")
                controlUrl = reader.readElementText().trimmed();
        } else if (reader.isEndElement()) {
            const QStringView name = reader.name();
            if (name == u"service") {
                inService = false;
                if (serviceType.contains(QLatin1String(":service:AVTransport:")) && !controlUrl.isEmpty()) {
                    QString owner;
                    for (qsizetype i = devices.size() - 1; i >= 0 && owner.isEmpty(); --i)
                        owner = devices[i];
                    return Renderer{owner.isEmpty() ? location.host() : owner, location, base.resolved(QUrl(controlUrl)), serviceType};
                }
            } else if (name == u"device" && !devices.isEmpty())
                devices.removeLast();
        }
    }
    return std::nullopt;
}

QByteArray soapEnvelope(const QString& serviceType, const QString& action, const SoapArgs& args)
{
    QString body;
    for (const auto& [key, value] : args)
        body += QStringLiteral("<%1>%2</%1>").arg(key, value.toHtmlEscaped());
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                          "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                          "<s:Body><u:%1 xmlns:u=\"%2\">%3</u:%1></s:Body></s:Envelope>")
        .arg(action, serviceType.toHtmlEscaped(), body)
        .toUtf8();
}

QString photoDidl(const QString& url, const QString& title)
{
    const QString protocol = QStringLiteral("http-get:*:image/jpeg:") + QLatin1String(kDlnaFlags);
    return QStringLiteral("<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
                          "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                          "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\">"
                          "<item id=\"0\" parentID=\"-1\" restricted=\"1\">"
                          "<dc:title>%1</dc:title>"
                          "<upnp:class>object.item.imageItem.photo</upnp:class>"
                          "<res protocolInfo=\"%2\">%3</res>"
                          "</item></DIDL-Lite>")
        .arg(title.toHtmlEscaped(), protocol.toHtmlEscaped(), url.toHtmlEscaped());
}

QString xmlValue(const QByteArray& xml, const QString& localName)
{
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == localName)
            return reader.readElementText().trimmed();
    }
    return {};
}

QHostAddress localAddressFor(const QHostAddress& peer)
{
    if (peer.isLoopback())
        return QHostAddress(QHostAddress::LocalHost);
    QHostAddress fallback;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            if (peer.isInSubnet(entry.ip(), entry.prefixLength()))
                return entry.ip();
            if (fallback.isNull())
                fallback = entry.ip();
        }
    }
    return fallback;
}

} // namespace cast
