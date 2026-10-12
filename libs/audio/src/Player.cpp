#include "audio/Player.h"

#include <windows.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
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

} // namespace

Player::~Player()
{
    pause();
}

bool Player::play(std::shared_ptr<const Clip> clip, size_t from, std::string& error)
{
    pause();
    if (!clip || clip->empty()) {
        error = "no hay audio";
        return false;
    }
    from = std::min(from, clip->frames() - 1);
    {
        std::lock_guard lock(m_mutex);
        m_started = m_ok = false;
        m_error.clear();
    }
    m_stop = false;
    m_position = from;
    m_playing = true;
    m_thread = std::thread([this, clip, from] { run(clip, from); });
    std::unique_lock lock(m_mutex);
    m_startedSignal.wait(lock, [this] { return m_started; });
    if (!m_ok) {
        error = m_error;
        lock.unlock();
        pause();
        return false;
    }
    return true;
}

void Player::pause()
{
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
    m_playing = false;
}

void Player::run(std::shared_ptr<const Clip> clip, size_t from)
{
    const auto started = [this](bool ok, std::string error) {
        {
            std::lock_guard lock(m_mutex);
            m_started = true;
            m_ok = ok;
            m_error = std::move(error);
        }
        m_startedSignal.notify_all();
    };
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    {
        ComPtr<IMMDeviceEnumerator> devices;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioRenderClient> render;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&devices));
        if (SUCCEEDED(hr))
            hr = devices->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (SUCCEEDED(hr))
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
        if (FAILED(hr)) {
            started(false, hresult("no hay salida de audio", hr));
        } else {
            // El formato del archivo, en float; Windows lo convierte al del dispositivo.
            WAVEFORMATEXTENSIBLE f{};
            f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            f.Format.nChannels = WORD(clip->channels);
            f.Format.nSamplesPerSec = DWORD(clip->sampleRate);
            f.Format.wBitsPerSample = 32;
            f.Format.nBlockAlign = WORD(clip->channels * 4);
            f.Format.nAvgBytesPerSec = f.Format.nSamplesPerSec * f.Format.nBlockAlign;
            f.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            f.Samples.wValidBitsPerSample = 32;
            f.dwChannelMask = clip->channels == 1 ? SPEAKER_FRONT_CENTER : SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            f.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            constexpr REFERENCE_TIME kBuffer = 1000000; // 100 ms
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                    kBuffer, 0, &f.Format, nullptr);
            UINT32 bufferFrames = 0;
            if (SUCCEEDED(hr))
                hr = client->SetEventHandle(event);
            if (SUCCEEDED(hr))
                hr = client->GetBufferSize(&bufferFrames);
            if (SUCCEEDED(hr))
                hr = client->GetService(IID_PPV_ARGS(&render));
            if (FAILED(hr)) {
                started(false, hresult("la salida de audio no acepta el formato", hr));
            } else {
                const size_t frames = clip->frames(), channels = size_t(clip->channels);
                size_t next = from; // el próximo cuadro a escribir
                const auto fill = [&] {
                    UINT32 padding = 0;
                    if (FAILED(client->GetCurrentPadding(&padding)))
                        return false;
                    const UINT32 room = bufferFrames - padding;
                    const UINT32 count = UINT32(std::min<size_t>(room, frames - next));
                    if (count > 0) {
                        BYTE* data = nullptr;
                        if (FAILED(render->GetBuffer(count, &data)))
                            return false;
                        std::memcpy(data, clip->samples.data() + next * channels, size_t(count) * channels * sizeof(float));
                        render->ReleaseBuffer(count, 0);
                        next += count;
                        padding += count;
                    }
                    m_position = std::min(frames - 1, next - padding);
                    return next < frames || padding > 0;
                };
                fill();
                const bool running = SUCCEEDED(client->Start());
                started(running, running ? std::string() : std::string("la salida de audio no arrancó"));
                while (running && !m_stop) {
                    WaitForSingleObject(event, 200);
                    if (!fill())
                        break; // terminó (o se perdió el dispositivo)
                }
                client->Stop();
                if (!m_stop)
                    m_position = frames - 1;
            }
        }
    }
    CloseHandle(event);
    if (SUCCEEDED(com))
        CoUninitialize();
    m_playing = false;
}

} // namespace audio
