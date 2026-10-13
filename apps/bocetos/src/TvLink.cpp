#include "TvLink.h"

#include <QElapsedTimer>
#include <QMetaObject>

TvLink::TvLink(appkit::Config& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
    m_pool.setMaxThreadCount(1);
    m_transport.onShown = [this](const QUrl& url, double ms) {
        const int sheet = m_sheetOfUrl.value(url.toString());
        qInfo().noquote() << QStringLiteral("TV: hoja %1 en el TV en %2 ms").arg(sheet).arg(ms, 0, 'f', 0);
        setStatus(QStringLiteral("mostrando la hoja %1").arg(sheet));
    };
    m_transport.onError = [this](const QString& error) {
        qWarning().noquote() << QStringLiteral("TV: no mostró la hoja (%1)").arg(error);
        setStatus(QStringLiteral("no mostró la hoja"));
        if (onWarn)
            onWarn(QStringLiteral("El TV no mostró la hoja"), error);
    };
    m_server.onRequest = [](const QString& method, const QString& path, int status, qint64 bytes) {
        qInfo().noquote() << QStringLiteral("TV: %1 %2 → %3 (%4 bytes)").arg(method, path).arg(status).arg(bytes);
    };
}

void TvLink::setStatus(const QString& status)
{
    m_status = status;
    if (onChanged)
        onChanged();
}

void TvLink::toggle()
{
    if (m_state != State::Off) {
        disconnect();
        return;
    }
    m_state = State::Searching;
    m_renderer = {};
    setStatus(QStringLiteral("buscando…"));
    // Primero el forzado o el último TV, directo; si no responde, se busca en la red.
    const QUrl direct = m_forced.isValid() ? m_forced : QUrl(m_config.value(QStringLiteral("tv")).toString());
    if (direct.isValid()) {
        qInfo().noquote() << QStringLiteral("TV: conectando a %1").arg(direct.toString());
        if (onBusy)
            onBusy(QStringLiteral("Conectando con el TV"), m_config.value(QStringLiteral("tvName")).toString());
        m_discovery.onFinished = [this](const QList<cast::Renderer>& renderers) { found(renderers, false); };
        m_discovery.fetch(direct);
        return;
    }
    if (onBusy)
        onBusy(QStringLiteral("Buscando TVs en la red…"), QStringLiteral("Tienen que estar prendidos y en el mismo wifi."));
    m_discovery.onFinished = [this](const QList<cast::Renderer>& renderers) { found(renderers, true); };
    m_discovery.search();
}

void TvLink::found(const QList<cast::Renderer>& renderers, bool searched)
{
    if (m_state != State::Searching)
        return;
    if (!searched && renderers.isEmpty()) {
        if (m_forced.isValid()) {
            if (onIdle)
                onIdle();
            m_state = State::Off;
            setStatus({});
            qWarning().noquote() << QStringLiteral("TV: no responde %1").arg(m_forced.toString());
            if (onWarn)
                onWarn(QStringLiteral("No responde el TV"), m_forced.toString());
            return;
        }
        qInfo() << "TV: el último no responde: se busca en la red";
        if (onBusy)
            onBusy(QStringLiteral("Buscando TVs en la red…"), QStringLiteral("Tienen que estar prendidos y en el mismo wifi."));
        m_discovery.onFinished = [this](const QList<cast::Renderer>& list) { found(list, true); };
        m_discovery.search();
        return;
    }
    if (onIdle)
        onIdle();
    if (renderers.isEmpty()) {
        m_state = State::Off;
        setStatus({});
        qWarning() << "TV: no se encontró ninguno";
        if (onWarn)
            onWarn(QStringLiteral("No encontré ningún TV"), QStringLiteral("Tiene que estar prendido y en el mismo wifi que esta compu."));
        return;
    }
    if (renderers.size() > 1 && onChoose) {
        qInfo().noquote() << QStringLiteral("TV: %1 en la red, a elegir").arg(renderers.size());
        onChoose(renderers);
        return;
    }
    connectTo(renderers.first());
}

void TvLink::choose(const cast::Renderer& renderer)
{
    if (m_state == State::Searching)
        connectTo(renderer);
}

void TvLink::connectTo(const cast::Renderer& renderer)
{
    const QHostAddress peer(renderer.host());
    // Con el TV de mentira (127.0.0.1) se escucha solo ahí: el firewall de Windows no pregunta.
    const QHostAddress listenOn = peer.isLoopback() ? QHostAddress(QHostAddress::LocalHost) : QHostAddress(QHostAddress::AnyIPv4);
    if (!m_server.listen(kPort, listenOn)) {
        m_state = State::Off;
        setStatus({});
        qWarning().noquote() << QStringLiteral("TV: no se pudo abrir el puerto %1 (%2)").arg(kPort).arg(m_server.errorString());
        if (onWarn)
            onWarn(QStringLiteral("No se pudo abrir el puerto %1").arg(kPort), m_server.errorString());
        return;
    }
    m_renderer = renderer;
    m_local = cast::localAddressFor(peer);
    m_transport.setRenderer(renderer);
    m_state = State::On;
    if (!m_forced.isValid()) { // el elegido queda para la próxima vez
        m_config.setValue(QStringLiteral("tv"), renderer.location.toString());
        m_config.setValue(QStringLiteral("tvName"), renderer.name);
    }
    qInfo().noquote() << QStringLiteral("TV: conectado a «%1» (%2), sirviendo desde %3:%4")
                             .arg(renderer.name, renderer.location.toString(), m_local.toString())
                             .arg(kPort);
    setStatus(QStringLiteral("conectado"));
    send();
}

void TvLink::disconnect()
{
    if (m_state == State::Off)
        return;
    m_discovery.cancel();
    if (m_state == State::On)
        m_transport.stop();
    m_state = State::Off;
    ++m_wantGeneration; // lo que se esté armando no se manda
    if (onIdle)
        onIdle();
    qInfo().noquote() << QStringLiteral("TV: cortado");
    setStatus({});
}

void TvLink::show(const bocetos::Page& page, const QColor& paper, const QString& title)
{
    m_want = {page, paper, title};
    m_hasWant = true;
    ++m_wantGeneration;
    if (m_state == State::On)
        send();
}

void TvLink::send()
{
    if (!m_hasWant)
        return;
    const Want want = m_want;
    const int generation = m_wantGeneration;
    setStatus(QStringLiteral("mandando la hoja %1…").arg(want.page.sheet));
    m_pool.start([this, want, generation] {
        QElapsedTimer time;
        time.start();
        const QByteArray jpeg = bocetos::jpegOf(bocetos::tvImage(want.page, want.paper));
        const double ms = time.nsecsElapsed() / 1e6;
        QMetaObject::invokeMethod(
            this,
            [this, want, generation, jpeg, ms] {
                if (generation != m_wantGeneration || m_state != State::On)
                    return; // ya se pidió otra hoja (o se cortó)
                const QUrl url = m_server.urlFor(m_server.publish(jpeg), m_local);
                m_sheetOfUrl.insert(url.toString(), want.page.sheet);
                qInfo().noquote() << QStringLiteral("TV: hoja %1 armada en %2 ms (%3 KB)").arg(want.page.sheet).arg(ms, 0, 'f', 0).arg(jpeg.size() / 1024);
                m_transport.show(url, want.title);
            },
            Qt::QueuedConnection);
    });
}
