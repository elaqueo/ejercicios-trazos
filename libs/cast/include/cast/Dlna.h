#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

#include <optional>

// cast (HU-145): mostrar una imagen en un TV de la red por DLNA/UPnP, como el "Transmitir a
// dispositivo" de Windows. El protocolo es el de pdfcast (probado con un Samsung QN85): buscar
// con SSDP, leer la descripción del TV, servir la imagen por HTTP y pedirle que la muestre por
// SOAP (AVTransport). Solo Qt Core y Network. Detalle en docs/bocetos/arquitectura.md.
namespace cast {

inline constexpr const char* kAvTransport = "urn:schemas-upnp-org:service:AVTransport:1";
// DLNA JPEG_LRG: hasta 4096 px por lado.
inline constexpr const char* kDlnaFlags = "DLNA.ORG_PN=JPEG_LRG;DLNA.ORG_OP=00;DLNA.ORG_FLAGS=00D00000000000000000000000000000";
inline constexpr int kMaxSide = 4096;
inline constexpr const char* kSsdpGroup = "239.255.255.250";
inline constexpr quint16 kSsdpPort = 1900;

// El M-SEARCH que pregunta por los que tienen AVTransport; `mx`: segundos que tienen para responder.
QByteArray searchMessage(int mx = 2);
// La LOCATION (la URL de la descripción) de una respuesta al M-SEARCH; vacía si no tiene.
QUrl locationOf(const QByteArray& response);

// Un TV (un MediaRenderer con AVTransport).
struct Renderer {
    QString name;        // friendlyName
    QUrl location;       // la descripción
    QUrl control;        // controlURL del AVTransport, absoluta
    QString serviceType; // el AVTransport que declara (:1, :2...)
    QString host() const { return location.host(); }
};
// El TV de una descripción UPnP (la del dispositivo que tiene el AVTransport), o nada.
std::optional<Renderer> parseDescription(const QByteArray& xml, const QUrl& location);

// SOAP: el sobre de una acción, con los argumentos en orden (los valores se escapan acá).
using SoapArgs = QList<QPair<QString, QString>>;
QByteArray soapEnvelope(const QString& serviceType, const QString& action, const SoapArgs& args);
// Los metadatos DIDL-Lite de una foto en `url` (CurrentURIMetaData).
QString photoDidl(const QString& url, const QString& title);
// El texto del primer elemento llamado `localName` (sin el prefijo), o vacío.
QString xmlValue(const QByteArray& xml, const QString& localName);

// La dirección de esta máquina que ve a `peer` (la de la misma subred); la del loopback para
// 127.0.0.1; si no, la primera IPv4 que no sea loopback.
QHostAddress localAddressFor(const QHostAddress& peer);

} // namespace cast
