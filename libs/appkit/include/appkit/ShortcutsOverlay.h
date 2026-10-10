#pragma once

#include "appkit/Shortcuts.h"

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

namespace appkit {

// Lista de atajos (HU-78, mesa "Atajos" del canvas de diseño): título con el nombre de la
// app, los grupos en sus columnas (Shortcuts::setColumns) y, en cada fila,
// las teclas como chips y qué hacen; los gestos que no son teclas en un gris más claro. Como
// el menú, es una ventana propia sin borde (un widget hijo quedaría tapado por la ventana
// nativa del lienzo). Ctrl+, o Esc la cierran.
class ShortcutsOverlay : public QWidget {
public:
    explicit ShortcutsOverlay(QWidget* owner);

    void setSheet(const QString& app, const QList<ShortcutGroup>& groups);

    std::function<void()> onClose;

    static constexpr int kWidth = 1040;
    static constexpr int kColumns = 3;
    static constexpr int kRowHeight = 32;

    // Índices de grupo de cada columna (para las pruebas).
    const QList<QList<int>>& columns() const { return m_columns; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int groupHeight(const ShortcutGroup& group) const;
    void closeOverlay();

    QString m_app;
    QList<ShortcutGroup> m_groups;
    QList<QList<int>> m_columns;
};

} // namespace appkit
