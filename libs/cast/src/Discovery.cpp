#include "cast/Discovery.h"

#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUdpSocket>

#include <algorithm>

namespace cast {

Discovery::Discovery(QObject* parent)
    : QObject(parent)
{
    m_deadline.setSingleShot(true);
    m_hardStop.setSingleShot(true);
    m_resend.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_searchOver = true;
        m_sockets.clear();
        finishIfDone();
    });
    connect(&m_hardStop, &QTimer::timeout, this, [this] { finish(); }); // una descripción que no llega
    connect(&m_resend, &QTimer::timeout, this, [this] {
        for (const auto& socket : m_sockets)
            socket->writeDatagram(searchMessage(), QHostAddress(QLatin1String(kSsdpGroup)), kSsdpPort);
    });
}

Discovery::~Discovery() = default;

void Discovery::search(int timeoutMs)
{
    cancel();
    m_running = true;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack) ||
            !(flags & QNetworkInterface::CanMulticast))
            continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            auto socket = std::make_unique<QUdpSocket>();
            if (!socket->bind(entry.ip(), 0))
                continue;
            socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 2);
            socket->setMulticastInterface(iface);
            QUdpSocket* raw = socket.get();
            const int generation = m_generation;
            connect(raw, &QUdpSocket::readyRead, this, [this, raw, generation, timeoutMs] {
                while (raw->hasPendingDatagrams()) {
                    const QUrl location = locationOf(raw->receiveDatagram().data());
                    if (generation == m_generation && location.isValid() && !m_seen.contains(location.toString())) {
                        m_seen.insert(location.toString());
                        describe(location, timeoutMs);
                    }
                }
            });
            raw->writeDatagram(searchMessage(), QHostAddress(QLatin1String(kSsdpGroup)), kSsdpPort);
            m_sockets.push_back(std::move(socket));
        }
    }
    qInfo().noquote() << QStringLiteral("TV: buscando por %1 interfaces").arg(m_sockets.size());
    m_resend.start(300);
    m_deadline.start(timeoutMs);
    m_hardStop.start(timeoutMs + 2500);
}

void Discovery::fetch(const QUrl& location, int timeoutMs)
{
    cancel();
    m_running = true;
    m_searchOver = true;
    m_seen.insert(location.toString());
    describe(location, timeoutMs);
    m_hardStop.start(timeoutMs + 500);
}

void Discovery::describe(const QUrl& location, int timeoutMs)
{
    QNetworkRequest request(location);
    request.setTransferTimeout(timeoutMs);
    QNetworkReply* reply = m_net.get(request);
    ++m_outstanding;
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, location, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        --m_outstanding;
        if (reply->error() == QNetworkReply::NoError) {
            if (const std::optional<Renderer> renderer = parseDescription(reply->readAll(), location)) {
                const bool known = std::any_of(m_found.cbegin(), m_found.cend(), [&](const Renderer& r) { return r.control == renderer->control; });
                if (!known) {
                    m_found.append(*renderer);
                    qInfo().noquote() << QStringLiteral("TV: «%1» en %2").arg(renderer->name, location.toString());
                }
            }
        } else {
            qInfo().noquote() << QStringLiteral("TV: no se pudo leer %1 (%2)").arg(location.toString(), reply->errorString());
        }
        finishIfDone();
    });
}

void Discovery::finishIfDone()
{
    if (m_running && m_searchOver && m_outstanding == 0)
        finish();
}

void Discovery::finish()
{
    if (!m_running)
        return;
    const QList<Renderer> found = m_found;
    cancel();
    if (onFinished)
        onFinished(found);
}

void Discovery::cancel()
{
    ++m_generation;
    m_running = false;
    m_searchOver = false;
    m_outstanding = 0;
    m_sockets.clear();
    m_deadline.stop();
    m_hardStop.stop();
    m_resend.stop();
    m_seen.clear();
    m_found.clear();
}

} // namespace cast
