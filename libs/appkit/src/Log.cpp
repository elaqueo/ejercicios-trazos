#include "appkit/Log.h"

#include "appkit/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

namespace appkit {

namespace {

QFile* g_logFile = nullptr;
QMutex g_logMutex;
QtMessageHandler g_previousHandler = nullptr;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "debug";
    case QtInfoMsg: return "info";
    case QtWarningMsg: return "warning";
    case QtCriticalMsg: return "critical";
    case QtFatalMsg: return "fatal";
    }
    return "?";
}

void writeToLog(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    {
        const QMutexLocker lock(&g_logMutex);
        QTextStream out(g_logFile);
        out << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")) << ' '
            << levelName(type) << ' ' << (context.category ? context.category : "default") << ": "
            << message << '\n';
        out.flush();
    }
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}

} // namespace

QString installFileLog(const QString& appName)
{
    const QString directory = familyDataDirectory();
    QDir().mkpath(directory);
    const QString path = directory + u'/' + appName + QStringLiteral(".log");

    auto* file = new QFile(path); // vive hasta el final del proceso
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "No se pudo abrir el log" << QDir::toNativeSeparators(path) << file->errorString();
        delete file;
        return {};
    }
    g_logFile = file;
    g_previousHandler = qInstallMessageHandler(writeToLog);
    return path;
}

} // namespace appkit
