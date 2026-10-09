#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <initializer_list>
#include <vector>

class QWidget;

namespace appkit {

// Una tecla con o sin Ctrl. key es un código de tecla virtual de Windows (VK_*, o la
// letra/número en mayúscula: 'R', '4').
struct KeyChord {
    unsigned key = 0;
    bool ctrl = false;

    bool operator==(const KeyChord&) const = default;
};

// Registro único de atajos (HU-14): el framework y la app declaran acá cada atajo con su
// nombre. Si dos acciones piden la misma tecla, gana la primera registrada y el choque queda
// anotado; reportConflicts() lo muestra al iniciar.
class Shortcuts {
public:
    struct Entry {
        QString name;
        std::vector<KeyChord> keys;
        std::function<void()> action;
    };

    // Una acción con una o más teclas (p. ej. '4' y VK_NUMPAD4). Devuelve false si alguna
    // tecla ya estaba tomada; esa tecla queda para la acción anterior.
    bool add(const QString& name, std::initializer_list<KeyChord> keys, std::function<void()> action);

    // Corre la acción de la tecla; false si la tecla no tiene atajo.
    bool trigger(unsigned key, bool ctrl) const;

    const std::vector<Entry>& entries() const { return m_entries; }
    // Un texto por choque: "Ctrl+R: «Repetir» ya es de «Otra cosa»".
    const QStringList& conflicts() const { return m_conflicts; }

    // Si hay choques, los escribe en el log y los muestra en un cartel sobre parent.
    void reportConflicts(QWidget* parent) const;

    // "Ctrl+S", "F10", "R", "[".
    static QString describe(KeyChord chord);

private:
    const Entry* find(KeyChord chord) const;

    std::vector<Entry> m_entries;
    QStringList m_conflicts;
};

} // namespace appkit
