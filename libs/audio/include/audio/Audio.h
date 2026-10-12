#pragma once

// audio: el audio de referencia de la toma (HU-106, Mesa de animación). Lee WAV y MP3 con Media
// Foundation (viene con Windows: sin dependencias nuevas) a PCM float, y calcula la onda para
// dibujarla. Sin Qt.

#include <string>
#include <vector>

namespace audio {

struct Clip {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> samples; // intercalados, de -1 a 1

    size_t frames() const { return channels > 0 ? samples.size() / size_t(channels) : 0; }
    double seconds() const { return sampleRate > 0 ? double(frames()) / sampleRate : 0; }
    bool empty() const { return frames() == 0; }
};

// Lee el archivo entero. false (con el motivo en error) si no se pudo. Inicializa COM y Media
// Foundation en el hilo que llama, si hace falta.
bool decode(const std::wstring& path, Clip& out, std::string& error);

// La onda: el pico (valor absoluto, de todos los canales) de cada una de `columns` partes
// iguales del audio. Vacía si no hay audio o columns <= 0.
std::vector<float> peaks(const Clip& clip, int columns);

} // namespace audio
