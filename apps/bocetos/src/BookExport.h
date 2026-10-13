#pragma once

#include "BocetosCore.h"

#include <QString>
#include <QStringList>

#include <functional>

// HU-140 y HU-141: exportar una hoja como PNG y el cuaderno entero como PDF (mesa "Bocetos · PDF del
// cuaderno").
namespace bocetos {

// "Bocetos diarios - hoja 07.png" (dos cifras; tres si el cuaderno tiene 100 hojas o más), sin los
// caracteres que Windows no acepta en un nombre de archivo.
QString sheetFileName(const notebooks::Notebook& notebook, int sheet);
// "Bocetos diarios.pdf".
QString pdfFileName(const notebooks::Notebook& notebook);
// `--exportar-en <carpeta>`: guarda ahí sin preguntar (la prueba de la app real); vacío si no está.
QString exportDirArgument(const QStringList& args);

// Copia la imagen de la hoja (terminada.png, o el borrador de Krita) a `file`, reemplazándolo.
bool exportSheet(const Page& page, const QString& file, QString* error = nullptr);

// Las páginas del PDF: las hojas terminadas, en orden (los borradores no entran).
QList<Page> pdfPages(const QString& dir, const notebooks::Notebook& notebook);
// El PDF: páginas del tamaño real de la hoja (325,1 × 203,2 mm, para imprimirlo a escala), la tapa
// primero (el color, la colección, el nombre, las fechas y cuántas hojas) y una hoja terminada por
// página, a toda la página y a 300 dpi. `progress` avisa después de cada hoja (desde el hilo que lo
// llama). Se puede llamar fuera del hilo de la app.
inline constexpr double kPdfPageWidthMm = 325.1, kPdfPageHeightMm = 203.2;
inline constexpr int kPdfDpi = 300;
bool writeNotebookPdf(const QString& file, const Book& book, const std::function<void(int done, int total)>& progress = {},
                      QString* error = nullptr);

} // namespace bocetos
