#include "audio/Audio.h"

#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace audio {

namespace {

std::string hresult(const char* what, HRESULT hr)
{
    char text[96];
    std::snprintf(text, sizeof text, "%s (0x%08lX)", what, static_cast<unsigned long>(hr));
    return text;
}

// Con Media Foundation ya arrancado: los objetos COM se sueltan al salir, antes de MFShutdown.
bool read(const std::wstring& path, Clip& out, std::string& error)
{
    ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
    if (FAILED(hr)) {
        error = hresult("no se pudo abrir el archivo", hr);
        return false;
    }
    reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);
    // Que lo entregue en float, con los canales y la frecuencia del archivo.
    ComPtr<IMFMediaType> wanted;
    MFCreateMediaType(&wanted);
    wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    hr = reader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, wanted.Get());
    if (FAILED(hr)) {
        error = hresult("el archivo no tiene audio que se pueda leer", hr);
        return false;
    }
    ComPtr<IMFMediaType> actual;
    reader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &actual);
    UINT32 channels = 0, rate = 0;
    actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    if (channels == 0 || rate == 0) {
        error = "formato de audio desconocido";
        return false;
    }
    out.channels = int(channels);
    out.sampleRate = int(rate);

    for (;;) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr)) {
            error = hresult("error al leer el audio", hr);
            out = {};
            return false;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
        if (!sample)
            continue;
        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer)))
            continue;
        BYTE* data = nullptr;
        DWORD length = 0;
        if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
            const size_t count = length / sizeof(float);
            const size_t at = out.samples.size();
            out.samples.resize(at + count);
            std::memcpy(out.samples.data() + at, data, count * sizeof(float));
            buffer->Unlock();
        }
    }
    out.samples.resize(out.frames() * size_t(out.channels)); // sin un cuadro a medias
    if (out.empty()) {
        error = "el audio está vacío";
        return false;
    }
    return true;
}

} // namespace

bool decode(const std::wstring& path, Clip& out, std::string& error)
{
    out = {};
    // COM ya puede estar inicializado en este hilo (Qt lo hace en el de la interfaz): se acepta
    // cualquier modelo.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comMine = SUCCEEDED(com);
    const HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    bool ok = false;
    if (FAILED(hr))
        error = hresult("Media Foundation no arrancó", hr);
    else {
        ok = read(path, out, error);
        MFShutdown();
    }
    if (comMine)
        CoUninitialize();
    return ok;
}

std::vector<float> peaks(const Clip& clip, int columns)
{
    std::vector<float> out;
    const size_t frames = clip.frames();
    if (frames == 0 || columns <= 0)
        return out;
    out.assign(size_t(columns), 0.0f);
    const size_t ch = size_t(clip.channels);
    for (int c = 0; c < columns; ++c) {
        const size_t from = frames * size_t(c) / size_t(columns);
        const size_t to = std::max(from + 1, frames * size_t(c + 1) / size_t(columns));
        float peak = 0;
        for (size_t i = from * ch; i < std::min(to, frames) * ch; ++i)
            peak = std::max(peak, std::abs(clip.samples[i]));
        out[size_t(c)] = std::min(peak, 1.0f);
    }
    return out;
}

} // namespace audio
