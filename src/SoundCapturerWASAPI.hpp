#pragma once

#ifdef ANALYZER_USE_WASAPI

#include <string>

#define NOMINMAX
#include <Windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>

class SoundCapturerWASAPI
{
public:

    SoundCapturerWASAPI() = default;

    bool init(size_t bufferSize, int samplingFrequency)
    {
        HRESULT hr = CoInitialize(nullptr);
        if (FAILED(hr))
        {
            std::cerr << "CoInitialize() failed" << std::endl;
            return false;
        }

        IMMDeviceEnumerator* pDeviceEnumerator = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pDeviceEnumerator);
        if (FAILED(hr))
        {
            std::cerr << "CoCreateInstance() failed" << std::endl;
            return false;
        }

        IMMDevice* pDevice = nullptr;
        hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr))
        {
            std::cerr << "GetDefaultAudioEndpoint() failed" << std::endl;
            return false;
        }

        IAudioClient* pAudioClient = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
        if (FAILED(hr))
        {
            std::cerr << "Activate() failed" << std::endl;
            return false;
        }

        wfx.nSamplesPerSec = samplingFrequency;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, &wfx, 0);
        if (FAILED(hr))
        {
            std::cerr << "Initialize() failed" << std::endl;
            return false;
        }

        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pAudioCaptureClient);
        if (FAILED(hr))
        {
            std::cerr << "GetService() failed" << std::endl;
            return false;
        }

        DWORD taskIndex = 0;
        HANDLE hTask = AvSetMmThreadCharacteristics("Audio", &taskIndex);
        if (!hTask)
        {
            std::cerr << "AvSetMmThreadCharacteristics() failed" << std::endl;
            return false;
        }

        hr = pAudioClient->Start();
        if (FAILED(hr))
        {
            std::cerr << "Start() failed" << std::endl;
            return false;
        }

        buffer.resize(bufferSize);

        return true;
    }

    void update()
    {
        UINT32 nextPacketSize;
        for (HRESULT hr = pAudioCaptureClient->GetNextPacketSize(&nextPacketSize); 0 < nextPacketSize; hr = pAudioCaptureClient->GetNextPacketSize(&nextPacketSize))
        {
            if (FAILED(hr))
            {
                std::cerr << "GetNextPacketSize() failed" << std::endl;
                return;
            }

            short* readData;
            UINT32 numFramesToRead;
            DWORD flags;
            hr = pAudioCaptureClient->GetBuffer(reinterpret_cast<BYTE**>(&readData), &numFramesToRead, &flags, nullptr, nullptr);
            if (FAILED(hr))
            {
                std::cerr << "GetBuffer() failed" << std::endl;
                return;
            }

            if (wfx.nChannels == 2 && wfx.wBitsPerSample == 16)
            {
                const LONG bytesLength = numFramesToRead * wfx.nBlockAlign;
                const auto count = bytesLength / sizeof(short);

                for (size_t i = 0; i < count; i += 2)
                {
                    buffer[currentHeadIndex] = readData[i] / 32767.0f;
                    ++currentHeadIndex;
                    currentHeadIndex %= buffer.size();
                }
            }
            else
            {
                std::cerr << "invalid format" << std::endl;
                return;
            }

            hr = pAudioCaptureClient->ReleaseBuffer(numFramesToRead);
            if (FAILED(hr))
            {
                std::cerr << "ReleaseBuffer() failed" << std::endl;
                return;
            }
        }
    }

    const std::vector<float>& getBuffer()const
    {
        return buffer;
    }

    size_t bufferHeadIndex()const
    {
        return currentHeadIndex;
    }

private:

    std::vector<float> buffer;
    size_t currentHeadIndex = 0;

    WAVEFORMATEX wfx = {
        .wFormatTag = WAVE_FORMAT_PCM,
        .nChannels = 2,
        .nSamplesPerSec = 48000,
        .nAvgBytesPerSec = 192000,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
        .cbSize = 0,
    };
    IAudioCaptureClient* pAudioCaptureClient = nullptr;
};

#endif
