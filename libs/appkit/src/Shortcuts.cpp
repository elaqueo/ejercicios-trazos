#include "appkit/Shortcuts.h"

#include <windows.h>

#include <QDebug>
#include <algorithm>
#include <QMessageBox>

namespace appkit {

bool Shortcuts::add(const QString& name, std::initializer_list<KeyChord> keys, std::function<void()> action)
{
    Entry entry{name, {}, std::move(action), m_group};
    bool ok = true;
    for (const KeyChord chord : keys) {
        if (const Entry* owner = find(chord)) {
            m_conflicts << QStringLiteral("%1: «%2» ya es de «%3»").arg(describe(chord), name, owner->name);
            ok = false;
        } else {
            entry.keys.push_back(chord);
        }
    }
    m_entries.push_back(std::move(entry));
    return ok;
}

void Shortcuts::joinWithPrevious(const QString& label)
{
    if (m_entries.size() < 2)
        return;
    m_entries.back().joined = true;
    m_entries[m_entries.size() - 2].label = label;
}

void Shortcuts::note(const QString& gesture, const QString& name)
{
    Entry entry;
    entry.name = name;
    entry.group = m_group;
    entry.gesture = gesture;
    m_entries.push_back(std::move(entry));
}

QList<ShortcutGroup> Shortcuts::sheet() const
{
    QList<ShortcutGroup> groups;
    const auto groupFor = [&groups](const QString& title) -> ShortcutGroup& {
        for (ShortcutGroup& g : groups)
            if (g.title == title)
                return g;
        groups.append({title, {}});
        return groups.last();
    };
    for (const Entry& entry : m_entries) {
        ShortcutGroup& group = groupFor(entry.group);
        if (!entry.gesture.isEmpty()) {
            group.rows.append({{entry.gesture}, entry.name, true});
            continue;
        }
        if (entry.keys.empty()) // todas sus teclas eran de otro: no tiene fila
            continue;
        const QString key = describe(entry.keys.front());
        if (entry.joined && !group.rows.isEmpty())
            group.rows.last().keys.append(key);
        else
            group.rows.append({{key}, entry.label.isEmpty() ? entry.name : entry.label, false});
    }
    // Por columna y, dentro de cada una, en el orden pedido; los que no figuran, al final de
    // la última, como aparecieron (stable_sort conserva el orden).
    const int last = std::max(0, int(m_columns.size()) - 1);
    QList<std::pair<int, ShortcutGroup>> ranked; // (posición, grupo)
    for (ShortcutGroup& g : groups) {
        g.column = last;
        int position = last * 1000 + 999;
        for (int column = 0; column < m_columns.size(); ++column)
            if (const qsizetype i = m_columns[column].indexOf(g.title); i >= 0) {
                g.column = column;
                position = column * 1000 + int(i);
                break;
            }
        ranked.append({position, g});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    groups.clear();
    for (const auto& [position, g] : ranked)
        groups.append(g);
    return groups;
}

bool Shortcuts::trigger(unsigned key, bool ctrl) const
{
    const Entry* entry = find({key, ctrl});
    if (!entry)
        return false;
    if (entry->action)
        entry->action();
    return true;
}

const Shortcuts::Entry* Shortcuts::find(KeyChord chord) const
{
    for (const Entry& entry : m_entries)
        for (const KeyChord& k : entry.keys)
            if (k == chord)
                return &entry;
    return nullptr;
}

void Shortcuts::reportConflicts(QWidget* parent) const
{
    if (m_conflicts.isEmpty())
        return;
    for (const QString& conflict : m_conflicts)
        qCritical().noquote() << "Atajo repetido:" << conflict;
    QMessageBox::warning(parent, QStringLiteral("Atajos repetidos"),
                         QStringLiteral("Hay teclas con dos acciones; queda la primera:\n\n%1")
                             .arg(m_conflicts.join(QLatin1Char('\n'))));
}

QString Shortcuts::describe(KeyChord chord)
{
    QString key;
    const unsigned k = chord.key;
    if (k >= VK_F1 && k <= VK_F24)
        key = QStringLiteral("F%1").arg(k - VK_F1 + 1);
    else if (k >= VK_NUMPAD0 && k <= VK_NUMPAD9)
        key = QStringLiteral("Num %1").arg(k - VK_NUMPAD0);
    else if ((k >= '0' && k <= '9') || (k >= 'A' && k <= 'Z'))
        key = QChar(char16_t(k));
    else {
        switch (k) {
        case VK_LEFT: key = QStringLiteral("←"); break;
        case VK_RIGHT: key = QStringLiteral("→"); break;
        case VK_UP: key = QStringLiteral("↑"); break;
        case VK_DOWN: key = QStringLiteral("↓"); break;
        case VK_OEM_4: key = QStringLiteral("["); break;
        case VK_OEM_6: key = QStringLiteral("]"); break;
        case VK_OEM_COMMA: key = QStringLiteral(","); break;
        case VK_OEM_PERIOD: key = QStringLiteral("."); break;
        case VK_OEM_MINUS: key = QStringLiteral("-"); break;
        case VK_OEM_PLUS: key = QStringLiteral("="); break;
        default: key = QStringLiteral("0x%1").arg(k, 2, 16, QLatin1Char('0'));
        }
    }
    return chord.ctrl ? QStringLiteral("Ctrl+") + key : key;
}

} // namespace appkit
