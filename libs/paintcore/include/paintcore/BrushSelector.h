#pragma once

#include "paintcore/BrushLibrary.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace paintcore {

// Selector de pinceles: grilla con la miniatura y el nombre de cada pincel, un
// filtro por carpeta (classic, deevad...) y un buscador. El pincel por defecto va
// primero. Al elegir uno (clic o Enter) emite brushSelected().
class BrushSelector : public QWidget {
    Q_OBJECT

public:
    explicit BrushSelector(QWidget* parent = nullptr);

    // La biblioteca debe vivir más que el selector.
    void setLibrary(const BrushLibrary* library);

    // Marca el pincel actual (sin emitir brushSelected).
    void setCurrentBrush(const QString& name);

    // Cantidad de pinceles visibles con el filtro actual (para pruebas).
    int visibleCount() const;

signals:
    // name es BrushPreset::name, o el de defaultBrushPreset().
    void brushSelected(const QString& name);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuild();
    void applyFilter();
    void choose(QListWidgetItem* item);

    const BrushLibrary* m_library = nullptr;
    QComboBox* m_folder = nullptr;
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
};

} // namespace paintcore
