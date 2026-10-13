#pragma once

#include <QDate>
#include <QString>

// HU-142, cuadernos de Krita: la hoja es un .kra (un zip: `mimetype` primero y sin comprimir,
// `maindoc.xml`, la imagen y una vista previa) con dos capas, el papel del cuaderno abajo (una capa
// de archivo bloqueada que apunta a ../papel.png y Krita agranda a la hoja, así cada hoja pesa poco)
// y una vacía encima para dibujar. Krita no sabe de Bocetos: lo que se guardó en Krita se lee de `mergedimage.png`, la
// imagen entera que Krita escribe al guardar, y la hoja se termina desde Bocetos.
namespace bocetos {

inline constexpr int kKritaDpi = 600;
// En papel.png, el tamaño de la hoja ("7679x4877"): Cartuchera escribe el papel a un cuarto de la
// resolución (lienzo::kPaperSizeKey) y Krita lo agranda.
inline constexpr const char* kPaperSizeKey = "Trazos-hoja";

// Escribe la hoja nueva en `kra` con el papel `paper` (papel.png; sin el tamaño, el de la imagen).
bool writeKra(const QString& kra, const QString& paper, QString* error = nullptr);
// Si la guardó Krita (Krita agrega documentinfo.xml; la que escribe Bocetos no lo tiene).
bool kraSavedByKrita(const QString& kra);
// `mergedimage.png` del .kra, escrito en `png` (entero o nada).
bool extractMerged(const QString& kra, const QString& png);

// El borrador al día: si Krita guardó la hoja después del último borrador.png, lo saca de nuevo.
// true si hay borrador.png.
bool refreshDraft(const QString& sheetDir);

// Terminar la hoja de Krita (Ctrl+Enter en el cuaderno): borrador.png (o el papel solo, si nunca se
// guardó en Krita) pasa a terminada.png, el .kra queda de solo lectura y el cuaderno, actualizado
// `today`.
bool finishKritaSheet(const QString& bookDir, int sheet, const QDate& today, QString* error = nullptr);

} // namespace bocetos
