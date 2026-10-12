#pragma once

#include <QList>
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

// Una fila de la lista de atajos (HU-78): las teclas como se dibujan ("Ctrl+S", "[", "]") y
// qué hacen. gesture: no es una tecla (Shift + arrastrar, la rueda de la tableta).
struct ShortcutRow {
    QStringList keys;
    QString name;
    bool gesture = false;
};

struct ShortcutGroup {
    QString title;
    QList<ShortcutRow> rows;
};

// Registro único de atajos (HU-14): el framework y la app declaran acá cada atajo con su
// nombre. Si dos acciones piden la misma tecla, gana la primera registrada y el choque queda
// anotado; reportConflicts() lo muestra al iniciar.
// La lista de atajos (Ctrl+, HU-78) sale de acá, así no se desactualiza: cada atajo cae en el
// grupo vigente al registrarlo (setGroup), dos acciones de un par se muestran en una fila
// (joinWithPrevious) y los gestos que no son teclas se anotan con note().
class Shortcuts {
public:
    struct Entry {
        QString name;
        std::vector<KeyChord> keys;
        std::function<void()> action;
        QString group;
        QString label;       // nombre en la lista si es distinto (el de un par)
        QString gesture;     // note(): el gesto, sin teclas ni acción
        bool joined = false; // va en la fila del anterior
    };

    // Grupo de los atajos que se registren desde ahora.
    void setGroup(const QString& group) { m_group = group; }
    // Orden de los grupos en la lista (HU-117: la página los reparte sola en columnas parejas).
    // Los que no figuran van al final, en orden de aparición.
    void setOrder(const QStringList& groups) { m_order = groups; }
    // El último atajo registrado se muestra en la fila del anterior, con este nombre ("[ ]
    // Tamaño de la mina" en vez de "Achicar" y "Agrandar").
    void joinWithPrevious(const QString& label);
    // Un gesto que no es tecla, solo para la lista ("Shift+arrastrar", "Girar libre").
    void note(const QString& gesture, const QString& name);

    // Una acción con una o más teclas (p. ej. '4' y VK_NUMPAD4). Devuelve false si alguna
    // tecla ya estaba tomada; esa tecla queda para la acción anterior.
    bool add(const QString& name, std::initializer_list<KeyChord> keys, std::function<void()> action);

    // Corre la acción de la tecla; false si la tecla no tiene atajo.
    bool trigger(unsigned key, bool ctrl) const;
    // Corre la acción por su nombre (el menú del lápiz, HU-116); false si no hay ninguna así.
    bool run(const QString& name) const;
    // La primera tecla de la acción como se dibuja ("Ctrl+Y", "←"); vacía si no hay acción así.
    QString keyOf(const QString& name) const;

    const std::vector<Entry>& entries() const { return m_entries; }
    // Un texto por choque: "Ctrl+R: «Repetir» ya es de «Otra cosa»".
    const QStringList& conflicts() const { return m_conflicts; }

    // Si hay choques, los escribe en el log y los muestra en un cartel sobre parent.
    void reportConflicts(QWidget* parent) const;

    // La lista para mostrar, por grupo. De cada acción se muestra su primera tecla ('4', no
    // también Num 4).
    QList<ShortcutGroup> sheet() const;

    // "Ctrl+S", "F10", "R", "[".
    static QString describe(KeyChord chord);

private:
    const Entry* find(KeyChord chord) const;

    std::vector<Entry> m_entries;
    QStringList m_conflicts;
    QString m_group;
    QStringList m_order;
};

} // namespace appkit
