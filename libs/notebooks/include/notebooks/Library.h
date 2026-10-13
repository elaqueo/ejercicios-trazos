#pragma once

#include "notebooks/Notebook.h"

#include <QList>
#include <QString>

// La biblioteca de Bocetos (HU-132): una carpeta raíz con una carpeta por cuaderno. Las
// colecciones son un campo del cuaderno, no carpetas.
namespace notebooks {

// Documentos\Trazos\Bocetos: son dibujos del usuario, no datos de la app, así que entran en sus
// backups (decidido el 13 de octubre de 2026).
QString defaultLibraryRoot();

// La carpeta que sale del nombre: minúsculas, sin acentos y con guiones ("Diseño de
// personajes" → "diseno-de-personajes"); "cuaderno" si no queda nada.
QString folderName(const QString& name);

struct Entry {
    QString dir;
    Notebook notebook;
};

// Los cuadernos de la raíz, por nombre de carpeta; los que no se pueden leer quedan en el log.
QList<Entry> listNotebooks(const QString& root);

// Crea la carpeta del cuaderno (con -2, -3… si el nombre choca) y su `cuaderno.json`; devuelve
// la carpeta, o vacío si no pudo.
QString createNotebook(const QString& root, const Notebook& notebook);

} // namespace notebooks
