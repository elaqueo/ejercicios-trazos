#include "appkit/ScreenChoice.h"

#include "appkit/Config.h"

#include <QGuiApplication>
#include <QScreen>

namespace appkit {

namespace {

const QString kManufacturer = QStringLiteral("manufacturer");
const QString kModel = QStringLiteral("model");
const QString kSerial = QStringLiteral("serial");
const QString kName = QStringLiteral("name");

} // namespace

ScreenId ScreenId::of(const QScreen* screen)
{
    return {screen->manufacturer(), screen->model(), screen->serialNumber(), screen->name()};
}

QVariantMap ScreenId::toVariant() const
{
    return {{kManufacturer, manufacturer}, {kModel, model}, {kSerial, serial}, {kName, name}};
}

std::optional<ScreenId> ScreenId::fromVariant(const QVariant& value)
{
    const QVariantMap map = value.toMap();
    if (map.isEmpty())
        return std::nullopt;
    return ScreenId{map.value(kManufacturer).toString(), map.value(kModel).toString(),
                    map.value(kSerial).toString(), map.value(kName).toString()};
}

QString ScreenId::describe() const
{
    // En Windows, Qt devuelve como nombre el mismo nombre comercial que el modelo.
    if (model.isEmpty() || model == name)
        return name;
    return QStringLiteral("%1 (%2)").arg(model, name);
}

std::optional<int> findScreen(const QList<ScreenId>& screens, const ScreenId& saved)
{
    auto find = [&](auto matches) -> int {
        for (int i = 0; i < screens.size(); ++i)
            if (matches(screens[i]))
                return i;
        return -1;
    };
    const auto sameModel = [&](const ScreenId& s) {
        return !saved.model.isEmpty() && s.manufacturer == saved.manufacturer && s.model == saved.model;
    };

    // Solo si el guardado no informa modelo se busca por nombre de Windows: si tiene
    // modelo y no aparece, está desconectado, aunque otro monitor haya heredado su nombre.
    int index = -1;
    if (saved.model.isEmpty())
        index = find([&](const ScreenId& s) { return !saved.name.isEmpty() && s.name == saved.name; });
    else if (!saved.serial.isEmpty())
        index = find([&](const ScreenId& s) { return sameModel(s) && s.serial == saved.serial; });
    else
        index = find([&](const ScreenId& s) { return sameModel(s) && s.name == saved.name; });
    return index < 0 ? std::nullopt : std::optional<int>(index);
}

QScreen* savedScreen(const Config& config)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (const auto saved = ScreenId::fromVariant(config.value(QStringLiteral("monitor"), {}, Config::Scope::Common))) {
        QList<ScreenId> ids;
        for (const QScreen* s : screens)
            ids.append(ScreenId::of(s));
        if (const auto index = findScreen(ids, *saved))
            return screens[*index];
    }
    return QGuiApplication::primaryScreen();
}

QScreen* nextScreen(Config& config, QScreen* current)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.size() < 2)
        return current;
    QScreen* next = screens[(screens.indexOf(current) + 1) % screens.size()];
    config.setValue(QStringLiteral("monitor"), ScreenId::of(next).toVariant(), Config::Scope::Common);
    return next;
}

} // namespace appkit
