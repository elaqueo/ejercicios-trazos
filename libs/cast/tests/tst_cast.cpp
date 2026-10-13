#include "FakeRenderer.h"

#include <cast/AvTransport.h>
#include <cast/Discovery.h>
#include <cast/Dlna.h>
#include <cast/MediaServer.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTest>

using namespace cast;

class TestCast : public QObject {
    Q_OBJECT

private slots:
    // El M-SEARCH y la LOCATION de la respuesta (como la de un Samsung).
    void busqueda()
    {
        const QByteArray msg = searchMessage(2);
        QVERIFY(msg.startsWith("M-SEARCH * HTTP/1.1\r\n"));
        QVERIFY(msg.contains("HOST: 239.255.255.250:1900\r\n"));
        QVERIFY(msg.contains("MAN: \"ssdp:discover\"\r\n"));
        QVERIFY(msg.contains("ST: urn:schemas-upnp-org:service:AVTransport:1\r\n"));
        QVERIFY(msg.endsWith("\r\n\r\n"));
        const QByteArray response = "HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=1800\r\nLocation: http://192.168.1.35:9197/dmr\r\n"
                                    "ST: urn:schemas-upnp-org:service:AVTransport:1\r\n\r\n";
        QCOMPARE(locationOf(response), QUrl(QStringLiteral("http://192.168.1.35:9197/dmr")));
        QVERIFY(locationOf("HTTP/1.1 200 OK\r\nST: x\r\n\r\n").isEmpty());
    }

    // La descripción: el nombre del dispositivo que tiene el AVTransport y su control absoluto.
    void descripcion()
    {
        const QByteArray xml =
            "<?xml version=\"1.0\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
            "<device><friendlyName>Router</friendlyName><deviceList>"
            "<device><friendlyName>[TV] Samsung QN85</friendlyName><serviceList>"
            "<service><serviceType>urn:schemas-upnp-org:service:RenderingControl:1</serviceType><controlURL>/rc</controlURL></service>"
            "<service><serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>"
            "<controlURL>upnp/control/AVTransport1</controlURL></service>"
            "</serviceList></device></deviceList></device></root>";
        const auto r = parseDescription(xml, QUrl(QStringLiteral("http://192.168.1.35:9197/dmr")));
        QVERIFY(r.has_value());
        QCOMPARE(r->name, QStringLiteral("[TV] Samsung QN85"));
        QCOMPARE(r->control, QUrl(QStringLiteral("http://192.168.1.35:9197/upnp/control/AVTransport1")));
        QCOMPARE(r->serviceType, QStringLiteral("urn:schemas-upnp-org:service:AVTransport:1"));
        QCOMPARE(r->host(), QStringLiteral("192.168.1.35"));
        // Con URLBase, el control sale de ahí; sin AVTransport, nada.
        const QByteArray based = "<root><URLBase>http://10.0.0.5:8080/</URLBase><device><friendlyName>TV</friendlyName><serviceList>"
                                 "<service><serviceType>urn:schemas-upnp-org:service:AVTransport:2</serviceType>"
                                 "<controlURL>/avt</controlURL></service></serviceList></device></root>";
        QCOMPARE(parseDescription(based, QUrl(QStringLiteral("http://10.0.0.5:1400/d.xml")))->control,
                 QUrl(QStringLiteral("http://10.0.0.5:8080/avt")));
        QVERIFY(!parseDescription("<root><device><friendlyName>Parlante</friendlyName></device></root>", QUrl()).has_value());
    }

    // El sobre SOAP y los datos de la foto, con lo que hay que escapar escapado.
    void soapYFoto()
    {
        const QByteArray soap = soapEnvelope(QLatin1String(kAvTransport), QStringLiteral("SetAVTransportURI"),
                                             {{QStringLiteral("InstanceID"), QStringLiteral("0")},
                                              {QStringLiteral("CurrentURI"), QStringLiteral("http://x/a.jpg?b=1&c=2")}});
        QVERIFY(soap.contains("<u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"));
        QVERIFY(soap.contains("<InstanceID>0</InstanceID><CurrentURI>http://x/a.jpg?b=1&amp;c=2</CurrentURI>"));
        QCOMPARE(xmlValue(soap, QStringLiteral("CurrentURI")), QStringLiteral("http://x/a.jpg?b=1&c=2"));
        const QString didl = photoDidl(QStringLiteral("http://x/1.jpg"), QStringLiteral("Bocetos <diarios> · hoja 7"));
        QVERIFY(didl.contains(QStringLiteral("<upnp:class>object.item.imageItem.photo</upnp:class>")));
        QVERIFY(didl.contains(QStringLiteral("protocolInfo=\"http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_LRG;")));
        QVERIFY(didl.contains(QStringLiteral("Bocetos &lt;diarios&gt; · hoja 7")));
    }

    // El servidor: GET y HEAD de lo publicado con las cabeceras DLNA, 404 lo demás, y se
    // guardan solo las últimas.
    void servidor()
    {
        MediaServer server;
        QVERIFY(server.listen(0, QHostAddress(QHostAddress::LocalHost)));
        const QString path = server.publish(QByteArray(1000, 'x'));
        QVERIFY(path.startsWith(QStringLiteral("/hoja/")) && path.endsWith(QStringLiteral("/1.jpg")));
        const QUrl url = server.urlFor(path, QHostAddress(QHostAddress::LocalHost));
        QNetworkAccessManager net;
        const auto request = [&](const QByteArray& verb, const QUrl& u) {
            QNetworkReply* r = net.sendCustomRequest(QNetworkRequest(u), verb);
            if (!QTest::qWaitFor([r] { return r->isFinished(); }, 5000))
                qWarning() << "sin respuesta:" << u;
            r->setParent(this); // se libera con la prueba
            return r;
        };
        QNetworkReply* get = request("GET", url);
        QCOMPARE(get->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        QCOMPARE(get->readAll().size(), 1000);
        QCOMPARE(get->header(QNetworkRequest::ContentTypeHeader).toString(), QStringLiteral("image/jpeg"));
        QCOMPARE(get->rawHeader("contentFeatures.dlna.org"), QByteArray(kDlnaFlags));
        QCOMPARE(get->rawHeader("transferMode.dlna.org"), QByteArray("Interactive"));
        QNetworkReply* head = request("HEAD", url);
        QCOMPARE(head->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        QCOMPARE(head->header(QNetworkRequest::ContentLengthHeader).toInt(), 1000);
        QNetworkReply* missing = request("GET", server.urlFor(QStringLiteral("/hoja/otro/1.jpg"), QHostAddress(QHostAddress::LocalHost)));
        QCOMPARE(missing->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 404);
        for (int i = 0; i < MediaServer::kKept; ++i)
            server.publish("y");
        QCOMPARE(request("GET", url)->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 404);
    }

    // De punta a punta contra el TV de mentira: encontrarlo, mostrar una imagen y que la descargue.
    void mostrarEnElTv()
    {
        FakeRenderer tv;
        QVERIFY(tv.listen());
        Discovery discovery;
        QList<Renderer> found;
        bool done = false;
        discovery.onFinished = [&](const QList<Renderer>& list) {
            found = list;
            done = true;
        };
        discovery.fetch(tv.location());
        QTRY_VERIFY(done);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].name, QStringLiteral("TV de prueba"));

        MediaServer server;
        QVERIFY(server.listen(0, QHostAddress(QHostAddress::LocalHost)));
        const QUrl url = server.urlFor(server.publish(QByteArray(2048, 'j')), localAddressFor(QHostAddress(found[0].host())));
        AvTransport transport;
        transport.setRenderer(found[0]);
        QStringList shown;
        transport.onShown = [&](const QUrl& u, double) { shown.append(u.toString()); };
        transport.show(url, QStringLiteral("hoja 1"));
        QTRY_COMPARE(shown.size(), 1);
        QCOMPARE(tv.events.value(0), QStringLiteral("SetAVTransportURI ") + url.toString());
        QVERIFY(tv.events.contains(QStringLiteral("Play")));
        QTRY_VERIFY(tv.events.contains(QStringLiteral("GET %1 200 2048 image/jpeg").arg(url.toString())));
    }

    // Las mañas: error y ya cargada (GetMediaInfo), error de verdad (Stop y reintento), 701.
    void manias()
    {
        FakeRenderer tv;
        QVERIFY(tv.listen());
        Renderer r{QStringLiteral("TV"), tv.location(), QUrl(QStringLiteral("http://127.0.0.1:%1/upnp/control/AVTransport1").arg(tv.port())),
                   QLatin1String(kAvTransport)};
        AvTransport transport;
        transport.setRenderer(r);
        int shown = 0;
        QStringList errors;
        transport.onShown = [&](const QUrl&, double) { ++shown; };
        transport.onError = [&](const QString& e) { errors.append(e); };

        tv.failSets = 1;
        tv.acceptSilently = true;
        transport.show(QUrl(QStringLiteral("http://127.0.0.1:1/a.jpg")), QStringLiteral("a"));
        QTRY_COMPARE(shown, 1);
        QCOMPARE(tv.events.mid(0, 3), (QStringList{QStringLiteral("SetAVTransportURI http://127.0.0.1:1/a.jpg"), QStringLiteral("GetMediaInfo"),
                                                    QStringLiteral("Play")}));
        tv.events.clear();
        tv.failSets = 1;
        tv.acceptSilently = false;
        tv.play701 = true;
        transport.show(QUrl(QStringLiteral("http://127.0.0.1:1/b.jpg")), QStringLiteral("b"));
        QTRY_COMPARE(shown, 2);
        QCOMPARE(tv.events.mid(0, 5), (QStringList{QStringLiteral("SetAVTransportURI http://127.0.0.1:1/b.jpg"), QStringLiteral("GetMediaInfo"),
                                                    QStringLiteral("Stop"), QStringLiteral("SetAVTransportURI http://127.0.0.1:1/b.jpg"),
                                                    QStringLiteral("Play")}));
        QVERIFY(errors.isEmpty());
        // Dos errores seguidos: se rinde y avisa.
        tv.events.clear();
        tv.failSets = 2;
        transport.show(QUrl(QStringLiteral("http://127.0.0.1:1/c.jpg")), QStringLiteral("c"));
        QTRY_COMPARE(errors.size(), 1);
        QVERIFY(errors[0].contains(QStringLiteral("714")));
        QCOMPARE(shown, 2);
        // Varias seguidas mientras manda: solo la primera y la última.
        tv.events.clear();
        transport.show(QUrl(QStringLiteral("http://127.0.0.1:1/1.jpg")), QStringLiteral("1"));
        transport.show(QUrl(QStringLiteral("http://127.0.0.1:1/2.jpg")), QStringLiteral("2"));
        transport.show(QUrl(QStringLiteral("http://127.0.0.1:1/3.jpg")), QStringLiteral("3"));
        QTRY_COMPARE(shown, 4);
        QCOMPARE(tv.events.filter(QStringLiteral("SetAVTransportURI")),
                 (QStringList{QStringLiteral("SetAVTransportURI http://127.0.0.1:1/1.jpg"), QStringLiteral("SetAVTransportURI http://127.0.0.1:1/3.jpg")}));
    }

    // La dirección propia que ve al TV.
    void direccionLocal()
    {
        QCOMPARE(localAddressFor(QHostAddress(QHostAddress::LocalHost)), QHostAddress(QHostAddress::LocalHost));
        const QHostAddress any = localAddressFor(QHostAddress(QStringLiteral("10.255.255.1")));
        QVERIFY(any.isNull() || any.protocol() == QAbstractSocket::IPv4Protocol);
    }
};

QTEST_GUILESS_MAIN(TestCast)
#include "tst_cast.moc"
