#pragma once

#include <notebooks/Notebook.h>

#include <QDate>
#include <QString>
#include <QStringList>

#include <optional>

// Cartuchera en modo cuaderno (HU-133): Bocetos la abre con `--hoja <carpeta>` sobre la hoja
// siguiente de un cuaderno (libs/notebooks); se dibuja con el papel del cuaderno, se guarda sola y
// Ctrl+Enter la termina. Sin --hoja, Cartuchera es la de siempre.
namespace cartuchera {

// La carpeta de `--hoja <carpeta>`; vacía si no está.
QString sheetArgument(const QStringList& args);
// HU-142: la carpeta del cuaderno de `--papel <carpeta>` (escribir papel.png, para los cuadernos de
// Krita); vacía si no está.
QString paperArgument(const QStringList& args);

struct NotebookSheet {
    QString notebookDir;
    QString sheetDir;
    int index = 0; // desde 1
    notebooks::Notebook notebook;
};

// La hoja de esa carpeta, si se puede dibujar en ella: tiene que ser una hoja-NNNN de un cuaderno,
// dentro del máximo, sin terminar y la siguiente (se llena en orden), y el cuaderno tiene que ser de
// Cartuchera (uno de Krita, HU-142, no). Si no, `error` dice por qué.
std::optional<NotebookSheet> notebookSheet(const QString& sheetDir, QString* error);

// "kraft", "tonado tostado"... (el papel de partida, para el aviso).
QString paperName(const notebooks::Paper& paper);

// Al terminar la hoja: la fecha de la última actualización del cuaderno.
bool markUpdated(const NotebookSheet& sheet, const QDate& today);

} // namespace cartuchera
