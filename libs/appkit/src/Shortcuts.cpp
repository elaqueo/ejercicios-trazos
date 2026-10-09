#include "appkit/Shortcuts.h"

#include <windows.h>

#include <QDebug>
#include <QMessageBox>

namespace appkit {

bool Shortcuts::add(const QString& name, std::initializer_list<KeyChord> keys, std::function<void()> action)
{
    Entry entry{name, {}, std::move(action)};
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
