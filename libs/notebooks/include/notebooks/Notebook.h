#pragma once

#include <QColor>
#include <QDate>
#include <QList>
#include <QString>

#include <optional>

// El cuaderno de Bocetos (HU-132): una carpeta con `cuaderno.json` y una carpeta por hoja
// (`hoja-0001/`, la toma de una sola hoja de Cartuchera). Lo usan Bocetos y Cartuchera en modo
// cuaderno; no depende del lienzo. El formato está en docs/bocetos/arquitectura.md.
namespace notebooks {

// 2: los cuadernos de Krita (HU-142), para que un Bocetos anterior no los abra en Cartuchera; los
// de Cartuchera se siguen escribiendo como 1.
inline constexpr int kNotebookVersion = 2;
inline constexpr const char* kNotebookFile = "cuaderno.json";
inline constexpr const char* kFinishedImage = "terminada.png";
// HU-142, cuadernos de Krita: el papel (en la carpeta del cuaderno), la hoja de Krita y lo último
// que se guardó en Krita (en la carpeta de la hoja).
inline constexpr const char* kPaperImage = "papel.png";
inline constexpr const char* kKritaFile = "hoja.kra";
inline constexpr const char* kDraftImage = "borrador.png";

// Con qué app se dibujan las hojas (HU-142): se elige al crear el cuaderno y no cambia.
enum class App { Cartuchera, Krita };

// El papel del cuaderno: de qué papel partió (preset) y los valores reales, que son los que
// valen; si un papel de partida cambia, los cuadernos que ya existen no.
struct Paper {
    QString preset;
    QColor color;
    int grain = 2047; // drymedia::PaperSpec::grain, 0..2047
    int texture = 20; // la textura visual del lienzo, 0..100
};

// Los papeles de partida: crema, blanco, kraft, tonado tostado y tonado gris.
QList<Paper> paperPresets();
// El de `preset`, o crema si no existe.
Paper paperPreset(const QString& preset);

struct Notebook {
    QString name;
    QString collection; // vacía: el estante general
    QColor cover;
    Paper paper = paperPreset(QStringLiteral("crema"));
    int maxSheets = 40;
    App app = App::Cartuchera;
    QDate started;
    QDate updated;
};

enum class LoadError { None, Missing, Corrupt, NewerVersion };

// Lee `cuaderno.json` de la carpeta del cuaderno; uno de una versión más nueva no se lee.
std::optional<Notebook> loadNotebook(const QString& dir, LoadError* error = nullptr);
// Lo escribe entero o no lo toca (QSaveFile).
bool saveNotebook(const QString& dir, const Notebook& notebook);

// El estado de una hoja sale de su carpeta: vacía (no existe), borrador (hay toma pero no
// imagen terminada) o terminada.
enum class SheetState { Empty, Draft, Finished };

// La carpeta de la hoja `index` (desde 1): <dir>/hoja-0001.
QString sheetFolder(const QString& dir, int index);
SheetState sheetState(const QString& dir, int index);
int finishedSheets(const QString& dir, const Notebook& notebook);
// La que abre "dibujar": el borrador o la primera vacía (se llena en orden); ninguna si está
// lleno.
std::optional<int> nextSheet(const QString& dir, const Notebook& notebook);

} // namespace notebooks
