#include "cast/AvTransport.h"

#include <QNetworkReply>
#include <QNetworkRequest>

namespace cast {

AvTransport::AvTransport(QObject* parent)
    : QObject(parent)
{
}

QString AvTransport::Reply::describe() const
{
    if (status == 0)
        return error.isEmpty() ? QStringLiteral("sin respuesta") : error;
    const QString code = xmlValue(body, QStringLiteral("errorCode"));
    const QString text = xmlValue(body, QStringLiteral("errorDescription"));
    QString out = QStringLiteral("HTTP %1").arg(status);
    if (!code.isEmpty())
        out += QStringLiteral(" · UPnP %1 %2").arg(code, text).trimmed();
    return out;
}

void AvTransport::call(const QString& action, const SoapArgs& args, int timeoutMs, std::function<void(const Reply&)> done)
{
    SoapArgs all{{QStringLiteral("InstanceID"), QStringLiteral("0")}};
    all += args;
    QNetworkRequest request(m_renderer.control);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("text/xml; charset=\"utf-8\""));
    request.setRawHeader("SOAPACTION", QStringLiteral("\"%1#%2\"").arg(m_renderer.serviceType, action).toUtf8());
    request.setTransferTimeout(timeoutMs);
    QNetworkReply* reply = m_net.post(request, soapEnvelope(m_renderer.serviceType, action, all));
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done = std::move(done), generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return; // stop() en el medio
        Reply r;
        r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        r.body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && r.status == 0)
            r.error = reply->errorString();
        done(r);
    });
}

void AvTransport::show(const QUrl& url, const QString& title)
{
    m_pending = std::make_pair(url, title);
    if (!m_busy)
        next();
}

void AvTransport::next()
{
    if (!m_pending)
        return;
    m_busy = true;
    m_url = m_pending->first;
    m_title = m_pending->second;
    m_pending.reset();
    m_time.start();
    set(false);
}

void AvTransport::set(bool retried)
{
    const QString url = m_url.toString();
    call(QStringLiteral("SetAVTransportURI"),
         {{QStringLiteral("CurrentURI"), url}, {QStringLiteral("CurrentURIMetaData"), photoDidl(url, m_title)}}, 15000,
         [this, url, retried](const Reply& r) {
             if (r.ok()) {
                 play();
                 return;
             }
             // A veces el TV la acepta y la respuesta no llega a tiempo: ¿qué tiene cargado?
             call(QStringLiteral("GetMediaInfo"), {}, 5000, [this, url, retried, r](const Reply& info) {
                 if (info.ok() && xmlValue(info.body, QStringLiteral("CurrentURI")) == url) {
                     play();
                     return;
                 }
                 if (retried) {
                     if (onError)
                         onError(QStringLiteral("SetAVTransportURI: %1").arg(r.describe()));
                     finish();
                     return;
                 }
                 call(QStringLiteral("Stop"), {}, 8000, [this](const Reply&) { set(true); }); // Stop y otra vez
             });
         });
}

void AvTransport::play()
{
    call(QStringLiteral("Play"), {{QStringLiteral("Speed"), QStringLiteral("1")}}, 8000, [this](const Reply& r) {
        // 701: el TV ya está mostrando la imagen.
        if (!r.ok() && xmlValue(r.body, QStringLiteral("errorCode")) != QLatin1String("701"))
            qWarning().noquote() << QStringLiteral("TV: Play: %1").arg(r.describe());
        if (onShown)
            onShown(m_url, m_time.nsecsElapsed() / 1e6);
        finish();
    });
}

void AvTransport::finish()
{
    m_busy = false;
    next();
}

void AvTransport::stop()
{
    ++m_generation;
    m_pending.reset();
    m_busy = false;
    if (m_renderer.control.isValid())
        call(QStringLiteral("Stop"), {}, 4000, [](const Reply&) {});
}

} // namespace cast
