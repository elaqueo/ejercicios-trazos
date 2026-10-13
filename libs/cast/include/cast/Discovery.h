#pragma once

#include "cast/Dlna.h"

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>

#include <functional>
#include <memory>
#include <vector>

class QUdpSocket;

namespace cast {

// Encuentra los TVs (HU-145): manda el M-SEARCH por cada interfaz IPv4 de la máquina (dos veces,
// por si se pierde un paquete UDP), junta las LOCATION que responden y lee la descripción de
// cada una. Al terminar el plazo avisa con los que tienen AVTransport. fetch() va directo a una
// descripción conocida (el último TV, o --tv), sin buscar.
class Discovery : public QObject {
public:
    explicit Discovery(QObject* parent = nullptr);
    ~Discovery() override;

    void search(int timeoutMs = 3000);
    void fetch(const QUrl& location, int timeoutMs = 4000);
    void cancel();
    bool running() const { return m_running; }

    std::function<void(const QList<Renderer>& found)> onFinished;

private:
    void describe(const QUrl& location, int timeoutMs);
    void finishIfDone();
    void finish();

    QNetworkAccessManager m_net;
    std::vector<std::unique_ptr<QUdpSocket>> m_sockets;
    QTimer m_deadline, m_hardStop, m_resend;
    QSet<QString> m_seen; // las LOCATION ya pedidas
    QList<Renderer> m_found;
    int m_outstanding = 0;
    int m_generation = 0;
    bool m_running = false, m_searchOver = false;
};

} // namespace cast
