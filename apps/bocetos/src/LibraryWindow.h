#pragma once

#include "BocetosCore.h"

#include <appkit/Config.h>
#include <appkit/Shortcuts.h>

#include <QDateTime>
#include <QPainter>
#include <QElapsedTimer>
#include <QString>
#include <QUrl>
#include <QWidget>

class NotebookDialog;
class Notice;
class TvChooser;
class TvLink;
class NotebookView;
class ThumbCache;
namespace appkit {
class ConfirmOverlay;
}

// La biblioteca de Bocetos (HU-135, mesa "Bocetos · biblioteca"): una ventana común con los
// cuadernos como libros apaisados en estantes por colección. Las flechas eligen, Enter
// o tocar abre el cuaderno (HU-137) encima, y Esc vuelve. Las teclas van al registro de atajos de
// appkit; las de la biblioteca no hacen nada con el cuaderno abierto, y al revés.
class LibraryWindow : public QWidget {
public:
    LibraryWindow(appkit::Shortcuts& shortcuts, appkit::Config& config);

    // Lee la biblioteca de `root` (queda en el log cuánto tardó, RNF-01) y elige el último
    // cuaderno elegido.
    void setLibrary(const QString& root, const QString& cartuchera);

    void move(bocetos::Move move);
    void openSelected();

    // HU-137: el cuaderno abierto, o nullptr si se ve la biblioteca.
    NotebookView* openNotebook() const;
    void closeNotebook();

    // HU-138: D dibuja la siguiente hoja del elegido en Cartuchera y espera; al volver relee el
    // cuaderno y, si la hoja quedó terminada, la muestra. Lleno: ofrece seguir en uno nuevo.
    void draw();
    // HU-142: krita.exe (vacío si no está); Ctrl+Enter en el cuaderno de Krita termina el borrador.
    void setKrita(const QString& krita);
    void finishSheet();

    // HU-140: Ctrl+E en el cuaderno abierto, la hoja que se mira como PNG; HU-141: Ctrl+P, el
    // cuaderno (abierto o elegido) como PDF, en segundo plano. `--exportar-en` guarda sin preguntar.
    void exportSheet();
    void exportPdf();
    void setExportDir(const QString& dir) { m_exportDir = dir; }

    // HU-145: T en el cuaderno abierto: la hoja también en el TV, o cortar. --tv fuerza un TV.
    void toggleTv();
    void setTvForced(const QUrl& location);

    // HU-136: Ctrl+N cuaderno nuevo (o seguir en uno nuevo si el elegido está lleno), F2 editarlo
    // (Ctrl+M con el foco en la colección), Supr mandarlo a la Papelera de reciclaje (con
    // confirmación).
    void newNotebook();
    void editSelected(bool focusCollection = false);
    void deleteSelected();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void drawn(int exitCode, bool crashed);
    void launch(const bocetos::DrawPlan& plan);
    void preparePaper(const bocetos::DrawPlan& plan);
    void relayout();
    void reload(const QString& selectFolder);
    bool dialogOpen() const;
    void select(int slot, bool log = true);
    void ensureVisible();
    const bocetos::Book* selectedBook() const;
    void preview();                       // HU-143: el panel del elegido y sus miniaturas
    void paintPanel(QPainter& p) const;
    int gridWidth() const;
    QString saveFile(const QString& title, const QString& name, const QString& filter);
    int contentTop() const;
    int viewBottom() const;

    appkit::Shortcuts& m_shortcuts;
    appkit::Config& m_config;
    QString m_root, m_cartuchera, m_krita;
    QString m_exportDir;     // --exportar-en
    bool m_exporting = false; // HU-141: armando el PDF
    QList<bocetos::Shelf> m_shelves;
    bocetos::Layout m_layout;
    int m_selected = -1;
    int m_scroll = 0;
    NotebookDialog* m_dialog = nullptr;
    NotebookView* m_view = nullptr;
    Notice* m_notice = nullptr;
    TvLink* m_tv = nullptr;
    TvChooser* m_chooser = nullptr;
    struct Drawing {
        QString dir, name;
        int sheet = 0; // 0: no se está dibujando
        // HU-142, en Krita: la hoja, si la carpeta es nueva (sin guardar se borra), cuándo se
        // escribió el .kra por última vez antes de abrirlo y desde cuándo está abierto.
        bool krita = false;
        QString kra;
        bool newSheet = false;
        QDateTime before;
        QElapsedTimer open;
    } m_drawing;
    appkit::ConfirmOverlay* m_confirm = nullptr;
    appkit::ConfirmOverlay* m_finish = nullptr; // HU-142: terminar el borrador de Krita
    // HU-143: el panel del elegido: lo usado, las últimas hojas y desde cuándo se esperan.
    ThumbCache* m_thumbs = nullptr;
    QString m_usage;
    QList<bocetos::Thumb> m_preview;
    QElapsedTimer m_previewSince;
    bool m_previewLogged = true;
    QString m_editing; // la carpeta del cuaderno que se edita
};
