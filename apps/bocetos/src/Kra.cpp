#include "Kra.h"

#include <notebooks/Notebook.h>

#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSaveFile>
#include <QUuid>

namespace bocetos {

namespace {

const QString kMerged = QStringLiteral("mergedimage.png");

QString uuid()
{
    return QUuid::createUuid().toString(QUuid::WithBraces);
}

// Las capas van de arriba hacia abajo: la del dibujo y, debajo, el papel (una capa de archivo
// bloqueada que Krita agranda al tamaño de la hoja), como las deja Krita al guardar.
QByteArray mainDoc(const QSize& size)
{
    return QStringLiteral(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE DOC PUBLIC '-//KDE//DTD krita 2.0//EN' 'http://www.calligra.org/DTD/krita-2.0.dtd'>
<DOC xmlns="http://www.calligra.org/DTD/krita" kritaVersion="5.2.0" syntaxVersion="2.0" editor="Krita">
 <IMAGE name="hoja" mime="application/x-kra" width="%1" height="%2" colorspacename="RGBA" profile="sRGB-elle-V2-srgbtrc.icc" x-res="%3" y-res="%3" description="">
  <layers>
   <layer name="Dibujo" nodetype="paintlayer" filename="layer2" x="0" y="0" opacity="255" visible="1" locked="0" colorspacename="RGBA" compositeop="normal" uuid="%4" collapsed="0" intimeline="0" channelflags="" selected="true"/>
   <layer name="Papel" nodetype="filelayer" source="../%6" scale="true" scalingmethod="1" filename="layer1" x="0" y="0" opacity="255" visible="1" locked="1" colorspacename="RGBA" compositeop="normal" uuid="%5" collapsed="0" intimeline="0" channelflags=""/>
  </layers>
 </IMAGE>
</DOC>
)")
        .arg(size.width())
        .arg(size.height())
        .arg(kKritaDpi)
        .arg(uuid(), uuid(), QString::fromLatin1(notebooks::kPaperImage))
        .toUtf8();
}

bool fail(QString* error, const QString& why)
{
    if (error)
        *error = why;
    return false;
}

} // namespace

bool writeKra(const QString& kra, const QString& paper, QString* error)
{
    QFile paperFile(paper);
    if (!paperFile.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("no se pudo leer %1").arg(paper));
    const QByteArray paperPng = paperFile.readAll();
    QImageReader reader(paper);
    const QSize small = reader.size();
    if (!small.isValid())
        return fail(error, QStringLiteral("%1 no es una imagen").arg(paper));
    // El tamaño de la hoja lo dice el papel (Cartuchera lo escribe a un cuarto de la resolución).
    const QStringList wh = reader.text(QString::fromLatin1(kPaperSizeKey)).split(QLatin1Char('x'));
    QSize size = wh.size() == 2 ? QSize(wh[0].toInt(), wh[1].toInt()) : QSize();
    if (size.isEmpty())
        size = small;
    reader.setScaledSize(small.scaled(256, 256, Qt::KeepAspectRatio));
    QByteArray preview;
    {
        QBuffer buffer(&preview);
        buffer.open(QIODevice::WriteOnly);
        reader.read().save(&buffer, "PNG");
    }

    // En memoria y después al archivo: QZipWriter cierra el dispositivo, y un QSaveFile no se cierra.
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&buffer);
        zip.setCompressionPolicy(QZipWriter::NeverCompress); // mimetype: primero y sin comprimir
        zip.addFile(QStringLiteral("mimetype"), "application/x-krita");
        zip.setCompressionPolicy(QZipWriter::AlwaysCompress);
        zip.addFile(QStringLiteral("maindoc.xml"), mainDoc(size));
        // La capa del dibujo, vacía: sin tiles.
        zip.addFile(QStringLiteral("hoja/layers/layer2"), "VERSION 2\nTILEWIDTH 64\nTILEHEIGHT 64\nPIXELSIZE 4\nDATA 0\n");
        zip.addFile(QStringLiteral("hoja/layers/layer2.defaultpixel"), QByteArray(4, '\0'));
        zip.setCompressionPolicy(QZipWriter::NeverCompress); // ya son PNG
        zip.addFile(kMerged, paperPng);
        zip.addFile(QStringLiteral("preview.png"), preview);
        zip.close();
        if (zip.status() != QZipWriter::NoError)
            return fail(error, QStringLiteral("no se pudo armar el zip"));
    }
    QSaveFile file(kra);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, file.errorString());
    return true;
}

bool kraSavedByKrita(const QString& kra)
{
    QZipReader zip(kra);
    if (!zip.isReadable())
        return false;
    const QList<QZipReader::FileInfo> files = zip.fileInfoList();
    return std::any_of(files.begin(), files.end(), [](const QZipReader::FileInfo& f) { return f.filePath == QLatin1String("documentinfo.xml"); });
}

bool extractMerged(const QString& kra, const QString& png)
{
    QZipReader zip(kra);
    if (!zip.isReadable())
        return false;
    const QByteArray data = zip.fileData(kMerged);
    if (!data.startsWith("\x89PNG"))
        return false;
    QSaveFile out(png);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(data);
    return out.commit();
}

bool refreshDraft(const QString& sheetDir)
{
    const QDir dir(sheetDir);
    const QFileInfo kra(dir.filePath(QString::fromLatin1(notebooks::kKritaFile)));
    const QFileInfo draft(dir.filePath(QString::fromLatin1(notebooks::kDraftImage)));
    if (kra.isFile() && (!draft.exists() || kra.lastModified() > draft.lastModified()) && kraSavedByKrita(kra.filePath())) {
        if (extractMerged(kra.filePath(), draft.filePath()))
            qInfo().noquote() << QStringLiteral("Krita: borrador al día en %1").arg(QDir::toNativeSeparators(draft.filePath()));
        else
            qWarning().noquote() << QStringLiteral("Krita: no se pudo leer la imagen de %1").arg(QDir::toNativeSeparators(kra.filePath()));
    }
    return QFileInfo::exists(draft.filePath());
}

bool finishKritaSheet(const QString& bookDir, int sheet, const QDate& today, QString* error)
{
    std::optional<notebooks::Notebook> nb = notebooks::loadNotebook(bookDir);
    if (!nb)
        return fail(error, QStringLiteral("%1 no es un cuaderno").arg(bookDir));
    if (notebooks::sheetState(bookDir, sheet) != notebooks::SheetState::Draft)
        return fail(error, QStringLiteral("la hoja %1 no es un borrador").arg(sheet));
    const QDir dir(notebooks::sheetFolder(bookDir, sheet));
    const QString source = refreshDraft(dir.path()) ? dir.filePath(QString::fromLatin1(notebooks::kDraftImage))
                                                    : QDir(bookDir).filePath(QString::fromLatin1(notebooks::kPaperImage));
    // Primero una copia aparte y después el nombre: terminada.png aparece entera o no aparece.
    const QString finished = dir.filePath(QString::fromLatin1(notebooks::kFinishedImage));
    const QString partial = finished + QStringLiteral(".parcial");
    QFile::remove(partial);
    if (!QFile::copy(source, partial) || !QFile::rename(partial, finished)) {
        QFile::remove(partial);
        return fail(error, QStringLiteral("no se pudo escribir %1").arg(finished));
    }
    QFile::setPermissions(dir.filePath(QString::fromLatin1(notebooks::kKritaFile)),
                          QFileDevice::ReadOwner | QFileDevice::ReadUser | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    nb->updated = today;
    if (!notebooks::saveNotebook(bookDir, *nb))
        qWarning() << "Krita: no se pudo actualizar la fecha del cuaderno";
    return true;
}

} // namespace bocetos
