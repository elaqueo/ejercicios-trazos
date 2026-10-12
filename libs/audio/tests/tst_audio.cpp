#include <audio/Audio.h>
#include <audio/Player.h>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>
#include <cmath>
#include <thread>
#include <cstdint>

namespace {

// Un WAV PCM de 16 bits: `seconds` de un seno de 440 Hz con amplitud `amplitude` en el primer
// canal y silencio en el resto.
bool writeWav(const QString& path, int rate, int channels, double seconds, double amplitude)
{
    const uint32_t frames = uint32_t(rate * seconds);
    const uint32_t bytes = frames * uint32_t(channels) * 2;
    QByteArray data;
    const auto u32 = [&](uint32_t v) { data.append(reinterpret_cast<const char*>(&v), 4); };
    const auto u16 = [&](uint16_t v) { data.append(reinterpret_cast<const char*>(&v), 2); };
    data.append("RIFF");
    u32(36 + bytes);
    data.append("WAVEfmt ");
    u32(16);
    u16(1);
    u16(uint16_t(channels));
    u32(uint32_t(rate));
    u32(uint32_t(rate * channels * 2));
    u16(uint16_t(channels * 2));
    u16(16);
    data.append("data");
    u32(bytes);
    for (uint32_t i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c) {
            const double v = c == 0 ? amplitude * std::sin(2 * 3.14159265358979 * 440 * i / rate) : 0;
            u16(uint16_t(int16_t(std::lround(v * 32767))));
        }
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

} // namespace

class TestAudio : public QObject {
    Q_OBJECT

private slots:
    // HU-106: un WAV se lee entero, con su frecuencia y sus canales.
    void leeUnWav()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("tono.wav"));
        QVERIFY(writeWav(path, 44100, 2, 0.5, 0.5));
        audio::Clip clip;
        std::string error;
        QVERIFY2(audio::decode(path.toStdWString(), clip, error), error.c_str());
        QCOMPARE(clip.sampleRate, 44100);
        QCOMPARE(clip.channels, 2);
        QCOMPARE(qint64(clip.frames()), qint64(22050));
        QVERIFY(std::abs(clip.seconds() - 0.5) < 1e-6);
        float peak = 0;
        for (size_t i = 0; i < clip.samples.size(); i += 2)
            peak = std::max(peak, std::abs(clip.samples[i]));
        QVERIFY(std::abs(peak - 0.5f) < 0.01f);
        QVERIFY(std::abs(clip.samples[1]) < 1e-4f); // el segundo canal, en silencio
    }

    // HU-107: un audio corto (en silencio) suena hasta el final en la salida de Windows; la
    // posición avanza y termina en el último cuadro. Pausar lo detiene donde estaba.
    void reproduce()
    {
        auto clip = std::make_shared<audio::Clip>();
        clip->sampleRate = 48000;
        clip->channels = 2;
        clip->samples.assign(size_t(48000 * 0.4) * 2, 0.0f);
        audio::Player player;
        std::string error;
        if (!player.play(clip, 0, error))
            QSKIP(qPrintable(QStringLiteral("sin salida de audio: %1").arg(QString::fromStdString(error))));
        QVERIFY(player.playing());
        for (int i = 0; i < 200 && player.playing(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        QVERIFY(!player.playing());
        QCOMPARE(player.position(), clip->frames() - 1);

        QVERIFY2(player.play(clip, 4800, error), error.c_str()); // desde 0,1 s
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        player.pause();
        QVERIFY(!player.playing());
        QVERIFY(player.position() > 4800 && player.position() < clip->frames() - 1);
    }

    void archivoQueNoExiste()
    {
        audio::Clip clip;
        std::string error;
        QVERIFY(!audio::decode(L"C:\\no-existe\\nada.wav", clip, error));
        QVERIFY(!error.empty());
        QVERIFY(clip.empty());
    }

    // La onda: el pico de cada parte; con silencio en la segunda mitad, esa mitad da 0.
    void onda()
    {
        audio::Clip clip;
        clip.sampleRate = 8;
        clip.channels = 1;
        clip.samples = {0.1f, -0.8f, 0.3f, 0.2f, 0, 0, 0, 0};
        const std::vector<float> p = audio::peaks(clip, 2);
        QCOMPARE(p.size(), size_t(2));
        QCOMPARE(p[0], 0.8f);
        QCOMPARE(p[1], 0.0f);
        QCOMPARE(audio::peaks(clip, 16).size(), size_t(16)); // más columnas que cuadros
        QVERIFY(audio::peaks(audio::Clip{}, 10).empty());
    }
};

QTEST_GUILESS_MAIN(TestAudio)
#include "tst_audio.moc"
