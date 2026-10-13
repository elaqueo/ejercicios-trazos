// El Krita de mentira (HU-142), para la prueba de la app real (scripts/smoke.ps1):
// `fake_krita [--nosplash] <hoja.kra>` hace lo que haría alguien en Krita: dibuja (un rectángulo
// oscuro en el centro de la imagen) y guarda como Krita (agrega documentinfo.xml), y sale.

#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QSaveFile>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QString kra = QCoreApplication::arguments().value(argc - 1);
    QList<QPair<QString, QByteArray>> entries;
    {
        QZipReader in(kra);
        if (!in.isReadable())
            return 1;
        for (const QZipReader::FileInfo& f : in.fileInfoList())
            if (f.isFile)
                entries.append({f.filePath, in.fileData(f.filePath)});
    }
    for (auto& [name, data] : entries) {
        if (name != QLatin1String("mergedimage.png"))
            continue;
        QImage image = QImage::fromData(data).convertToFormat(QImage::Format_RGB32);
        if (image.isNull())
            return 1;
        for (int y = image.height() / 3; y < image.height() * 2 / 3; ++y) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = image.width() / 3; x < image.width() * 2 / 3; ++x)
                line[x] = qRgb(0x2A, 0x2A, 0x2A);
        }
        data.clear();
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
    }
    entries.append({QStringLiteral("documentinfo.xml"), "<?xml version=\"1.0\"?><document-info/>"});
    QByteArray bytes; // en memoria: QZipWriter cierra el dispositivo, y un QSaveFile no se cierra
    QBuffer out(&bytes);
    out.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&out);
        for (const auto& [name, data] : entries) {
            zip.setCompressionPolicy(name == QLatin1String("mimetype") ? QZipWriter::NeverCompress : QZipWriter::AlwaysCompress);
            zip.addFile(name, data);
        }
        zip.close();
    }
    QSaveFile file(kra);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() ? 0 : 1;
}
