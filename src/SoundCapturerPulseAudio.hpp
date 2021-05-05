#pragma once

#ifdef ANALYZER_USE_PULSEAUDIO

#include <vector>
#include <string>
#include <iostream>

#include <pulse/pulseaudio.h>
#include <pulse/error.h>

class SoundCapturerPulseAudio
{
public:

    SoundCapturerPulseAudio() = default;

    bool init(size_t bufferSize, int samplingFrequency)
    {
        const auto contextStateCallback = [](pa_context* context, void* userdata)
        {
            const auto callback = [](pa_context* context, const pa_server_info* info, void* userdata)
            {
                auto pData = reinterpret_cast<UserData*>(userdata);

                pData->sinkName = std::string(info->default_sink_name) + std::string(".monitor");

                const pa_buffer_attr recAttr = {
                    .maxlength = static_cast<std::uint32_t>(-1),
                    .tlength = static_cast<std::uint32_t>(-1),
                    .minreq = static_cast<std::uint32_t>(-1),
                    .fragsize = static_cast<std::uint32_t>(pa_usec_to_bytes(50 * PA_USEC_PER_MSEC, &pData->ss)),
                };

                if (pa_stream_connect_record(pData->stream, pData->sinkName.c_str(), &recAttr, PA_STREAM_ADJUST_LATENCY) != 0)
                {
                    std::cerr << "pa_stream_connect_record() failed" << std::endl;
                }
            };

            const auto streamReadCallback = [](pa_stream* s, size_t bytesLength, void* userdata)
            {
                auto pData = reinterpret_cast<UserData*>(userdata);

                while (pa_stream_readable_size(s) > 0)
                {
                    const void *data;
                    if (pa_stream_peek(s, &data, &bytesLength) < 0)
                    {
                        std::cerr << "pa_stream_peek() failed" << std::endl;
                        return;
                    }

                    assert(bytesLength % 4 == 0);

                    if (pData->buffer.size() < bytesLength / 4)
                    {
                        pData->buffer.resize(bytesLength / 4);
                    }

                    if (data)
                    {
                        const size_t bufferCount = pData->buffer.size();
                        const auto readData = static_cast<const std::int16_t*>(data);

                        for (size_t i = 0; i < bytesLength / 2; i += 2)
                        {
                            pData->buffer[pData->bufferHeadIndex] = readData[i] / 32767.0f;
                            ++pData->bufferHeadIndex;
                            pData->bufferHeadIndex %= bufferCount;
                        }
                        pData->readCount += bytesLength / 2;
                    }

                    pa_stream_drop(s);
                }
            };

            auto pData = reinterpret_cast<UserData*>(userdata);

            switch (pa_context_get_state(context))
            {
            case PA_CONTEXT_READY:
            {
                pData->stream = pa_stream_new(context, "minimal spectrum analyzer", &pData->ss, nullptr);
                if (!pData->stream)
                {
                    std::cerr << "pa_stream_new() failed" << std::endl;
                    return;
                }

                pa_operation_unref(pa_context_get_server_info(context, callback, userdata));
                pa_stream_set_read_callback(pData->stream, streamReadCallback, userdata);
                break;
            }

            case PA_CONTEXT_FAILED:
                std::cerr << "error: PA_CONTEXT_FAILED" << std::endl;
                break;

            case PA_CONTEXT_TERMINATED:
                pa_mainloop_quit(pData->mainloop, 0);
                pData->mainloop = nullptr;
                break;

            default: break;
            }
        };

        data.ss.rate = static_cast<std::uint32_t>(samplingFrequency);

        const std::string appName = std::string("minimal spectrum analyzer");

        pa_mainloop_api* mainloop_api = pa_mainloop_get_api(data.mainloop);

        pa_context* context = pa_context_new(mainloop_api, appName.c_str());

        pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

        pa_context_set_state_callback(context, contextStateCallback, static_cast<void*>(&data));

        pa_mainloop_iterate(data.mainloop, 0, nullptr);

        data.buffer.resize(bufferSize);

        return true;
    }

    void update()
    {
        if (data.mainloop)
        {
            pa_mainloop_iterate(data.mainloop, 0, nullptr);
        }
    }

    const std::vector<float>& getBuffer()const
    {
        return data.buffer;
    }

    size_t bufferHeadIndex()const
    {
        return data.bufferHeadIndex;
    }

    size_t bufferReadCount()const
    {
        return data.readCount;
    }

private:

    struct UserData
    {
        UserData()
            : mainloop(pa_mainloop_new())
        {}

        pa_sample_spec ss = {
            .format = PA_SAMPLE_S16LE,
            .rate = 44100,
            .channels = 2,
        };

        std::string sinkName;
        std::vector<float> buffer;
        size_t bufferHeadIndex = 0;
        size_t readCount = 0;

        pa_stream* stream = nullptr;
        pa_mainloop* mainloop = nullptr;
    };

    UserData data;
};

#endif
