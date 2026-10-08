#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

#include <optional>

class QScreen;

namespace appkit {

// Identidad de un monitor. El orden y los nombres que da el sistema pueden cambiar al
// reconectar los monitores, así que lo que identifica es fabricante + modelo + serie;
// el nombre (QScreen::name) queda como respaldo para monitores que no informan modelo.
struct ScreenId {
    QString manufacturer;
    QString model;
    QString serial;
    QString name;

    static ScreenId of(const QScreen* screen);

    QVariantMap toVariant() const;
    static std::optional<ScreenId> fromVariant(const QVariant& value);

    // Para el log: el modelo, y el nombre si es distinto.
    QString describe() const;
};

// Índice del monitor guardado entre los conectados: busca por fabricante + modelo +
// serie; si no tiene serie, por fabricante + modelo + nombre; si no informa modelo,
// por nombre de Windows. nullopt si no está conectado.
std::optional<int> findScreen(const QList<ScreenId>& screens, const ScreenId& saved);

} // namespace appkit
