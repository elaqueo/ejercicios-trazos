// El TV de mentira como programa (HU-145), para la prueba de la app real: Bocetos se conecta con
// --tv http://127.0.0.1:<puerto>/dmr y nada se manda al TV de verdad.
//   fake_renderer --port 8799 --log <archivo> [--fail-first] [--play-701]
// Cada acción va a la consola y al archivo, una por línea.

#include "FakeRenderer.h"

#include <QCoreApplication>
#include <QFile>

#include <cstdio>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    const auto value = [&](const QString& name) {
        const qsizetype at = args.indexOf(name);
        return at >= 0 && at + 1 < args.size() ? args[at + 1] : QString();
    };
    FakeRenderer tv;
    tv.failSets = args.contains(QStringLiteral("--fail-first")) ? 1 : 0;
    tv.play701 = args.contains(QStringLiteral("--play-701"));
    if (!tv.listen(quint16(value(QStringLiteral("--port")).toUInt()))) {
        std::fprintf(stderr, "fake_renderer: no se pudo escuchar\n");
        return 1;
    }
    QFile log(value(QStringLiteral("--log")));
    if (!log.fileName().isEmpty() && !log.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::fprintf(stderr, "fake_renderer: no se pudo abrir el log\n");
        return 1;
    }
    tv.onEvent = [&](const QString& text) {
        std::printf("%s\n", text.toUtf8().constData());
        std::fflush(stdout);
        if (log.isOpen()) {
            log.write(text.toUtf8() + '\n');
            log.flush();
        }
    };
    std::printf("TV de prueba en %s\n", tv.location().toString().toUtf8().constData());
    std::fflush(stdout);
    return QCoreApplication::exec();
}
