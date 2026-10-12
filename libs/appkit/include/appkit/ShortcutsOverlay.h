#pragma once

#include "appkit/Shortcuts.h"

#include <QList>
#include <QSize>
#include <QString>
#include <QWidget>

#include <functional>

namespace appkit {

// Lista de atajos (HU-78; rediseñada en HU-117, mesa "Atajos" del canvas de diseño): título
// con el nombre de la app y la leyenda, y los grupos en el orden del registro
// (Shortcuts::setOrder), repartidos solos en cuatro columnas parejas. En cada fila, las teclas
// como chips y qué hacen; los gestos que no son teclas, en chips de borde punteado. Entra
// siempre en el lugar que hay: si no alcanza el alto, las filas se achican. Como el menú, es
// una ventana propia sin borde (un widget hijo quedaría tapado por la ventana nativa del
// lienzo). Ctrl+, o Esc la cierran.
class ShortcutsOverlay : public QWidget {
public:
    explicit ShortcutsOverlay(QWidget* owner);

    // available: el lugar que hay, en px de Qt (vacío: sin límite).
    void setSheet(const QString& app, const QList<ShortcutGroup>& groups, const QSize& available = {});

    std::function<void()> onClose;

    static constexpr int kWidth = 1400;
    static constexpr int kColumns = 4;
    static constexpr int kRowHeight = 30;    // la de la mesa
    static constexpr int kMinRowHeight = 22; // hasta acá se achica para entrar

    // Columnas parejas: corta la lista de alturas (en orden) en a lo sumo `columns` tramos
    // seguidos, de modo que el más alto sea lo más bajo posible; `gap` va entre dos seguidos.
    // Devuelve siempre `columns` listas de índices (las últimas pueden quedar vacías).
    static QList<QList<int>> balance(const QList<int>& heights, int columns, int gap);

    // Índices de grupo de cada columna y alto de fila elegido (para las pruebas).
    const QList<QList<int>>& columns() const { return m_columns; }
    int rowHeight() const { return m_rowHeight; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int groupHeight(const ShortcutGroup& group, int rowHeight) const;
    void closeOverlay();

    QString m_app;
    QList<ShortcutGroup> m_groups;
    QList<QList<int>> m_columns;
    int m_rowHeight = kRowHeight;
    bool m_gestures = false; // hay gestos: la leyenda
};

} // namespace appkit
