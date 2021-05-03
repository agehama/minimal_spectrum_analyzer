#include <pulse/pulseaudio.h>
#include <pulse/error.h>

#include <fft.h>
#include <fft_internal.h>

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cmath>

class SpectrumAnalyzer
{
public:

    SpectrumAnalyzer() = default;

    SpectrumAnalyzer(size_t fftSampleSize, int samplingFrequency)
    {
        init(fftSampleSize, samplingFrequency);
    }

    ~SpectrumAnalyzer()
    {
        mufft_free(input);
        mufft_free(input2);
        mufft_free(output);
        mufft_free_plan_1d(muplan);
    }

    void init(size_t fftSampleSize, int samplingFrequency)
    {
        sampleSize = fftSampleSize;
        unitFreq = samplingFrequency / static_cast<float>(sampleSize);

        input = static_cast<float*>(mufft_alloc(sampleSize * sizeof(float)));
        input2 = static_cast<float*>(mufft_alloc(sampleSize * sizeof(float)));
        output = static_cast<cfloat*>(mufft_alloc(sampleSize * sizeof(cfloat)));
        muplan = mufft_create_plan_1d_r2c(sampleSize, MUFFT_FLAG_CPU_ANY);
    }

    void update(const std::vector<std::int16_t>& buffer, size_t headIndex)
    {
        assert(sampleSize * 2 == buffer.size());

        const float coef = 1.0f / 32767.0f;
        for (size_t i = 0; i < sampleSize; ++i)
        {
            input[i] = coef * buffer[(headIndex + i*2) % buffer.size()];
        }

        executeFFT();

        updateSpectrum();
    }

    const std::vector<float>& spectrum()const
    {
        return spectrumView;
    }

private:

    void executeFFT()
    {
        const float pi = 3.1415926535f;
        for (int i = 0; i < sampleSize; ++i)
        {
            const float t = 1.0f * i / (sampleSize - 1);
            const float hammingWindow = (0.54f - 0.46f * std::cos(2.0f * pi * t));
            input2[i] = hammingWindow * input[i];
        }

        mufft_execute_plan_1d(muplan, output, input2);
    }

    void updateSpectrum()
    {
        const size_t outputSize = sampleSize / 2;
        const float normalizeCoef = 2.0f / outputSize;

        const float logBase = 10.0f;
        const float freqMin = 30;
        const float freqMax = 5000;
        const float logFreqMin = std::pow(freqMin, 1.0f / logBase);
        const float logFreqMax = std::pow(freqMax, 1.0f / logBase);

        const auto logScaledFreq = [&](float t)
        {
            const float logFreq = logFreqMin + (logFreqMax - logFreqMin) * t;
            const float freq = std::pow(logFreq, logBase);
            return freq;
        };

        const auto getPower = [&](size_t index)
        {
            const cfloat a = output[index];
            float rx = normalizeCoef * a.real;
            float ix = normalizeCoef * a.imag;
            return std::sqrt(rx * rx + ix * ix);
        };

        const float dBsplMin = 40.0f;
        const float dBsplMax = 75.0f;
        const float p0 = 20 * 1.0e-5f;

        spectrumView.resize(outputSize - 1);
        for (size_t i = 0; i < spectrumView.size(); ++i)
        {
            const float t0 = 1.0 * i / spectrumView.size();
            const float t1 = 1.0 * (i + 1) / spectrumView.size();

            const int index0 = static_cast<int>(std::floor(logScaledFreq(t0) / unitFreq));
            const int index1 = static_cast<int>(std::floor(logScaledFreq(t1) / unitFreq));

            float pressureMax = getPower(index0);
            for (int j = index0 + 1; j < index1; ++j)
            {
                pressureMax = std::max(pressureMax, getPower(j));
            }

            const float f = unitFreq * i;
            //d weighting
            const float hf = ((1037918.48f - f * f) * (1037918.48f - f * f) + 1080768.16f * f * f) /
                ((9837328.0f - f * f) * (9837328.0f - f * f) + 11723776.0f * f * f);
            const float rd = (f / (6.8966888496476f * 1.0e-5f)) * std::sqrt(hf / ((f * f + 79919.29f) * (f * f + 1345600.0f)));
            const float spl = 20.0f * std::log10(rd * pressureMax / p0);

            const float loudness = std::max(0.0f, (spl - dBsplMin)) / (dBsplMax - dBsplMin);
            spectrumView[i] = loudness;
        }
    }

    std::vector<float> spectrumView;

    float* input = nullptr;
    float* input2 = nullptr;
    cfloat* output = nullptr;
    mufft_plan_1d* muplan = nullptr;

    size_t sampleSize = 0;
    float unitFreq = 0.0f;
};

class SoundCapturer
{
public:

    SoundCapturer() = default;

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

                    if (pData->buffer.size() < bytesLength)
                    {
                        pData->buffer.resize(bytesLength);
                    }

                    if (data)
                    {
                        const size_t head = pData->bufferHeadIndex;
                        const size_t bufferCount = pData->buffer.size();
                        const auto readData = static_cast<const std::int16_t*>(data);

                        for (size_t i = 0; i * 2 < bytesLength; ++i)
                        {
                            pData->buffer[(head + i) % bufferCount] = readData[i];
                        }
                    }

                    pData->bufferHeadIndex += bytesLength / 2;
                    pData->bufferHeadIndex %= pData->buffer.size();

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

    const std::vector<std::int16_t>& getBuffer()const
    {
        return data.buffer;
    }

    size_t bufferHeadIndex()const
    {
        return data.bufferHeadIndex;
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
        std::vector<std::int16_t> buffer;
        size_t bufferHeadIndex = 0;

        pa_stream* stream = nullptr;
        pa_mainloop* mainloop = nullptr;
    };

    UserData data;
};

class Renderer
{
public:

    Renderer() = default;

    Renderer(size_t width)
        : width(width)
    {}

    void draw(const std::vector<float>& values)const
    {
        const std::string str("⠀⠁⠂⠃⠄⠅⠆⠇⡀⡁⡂⡃⡄⡅⡆⡇⠈⠉⠊⠋⠌⠍⠎⠏⡈⡉⡊⡋⡌⡍⡎⡏⠐⠑⠒⠓⠔⠕⠖⠗⡐⡑⡒⡓⡔⡕⡖⡗⠘⠙⠚⠛⠜⠝⠞⠟⡘⡙⡚⡛⡜⡝⡞⡟⠠⠡⠢⠣⠤⠥⠦⠧⡠⡡⡢⡣⡤⡥⡦⡧⠨⠩⠪⠫⠬⠭⠮⠯⡨⡩⡪⡫⡬⡭⡮⡯⠰⠱⠲⠳⠴⠵⠶⠷⡰⡱⡲⡳⡴⡵⡶⡷⠸⠹⠺⠻⠼⠽⠾⠿⡸⡹⡺⡻⡼⡽⡾⡿⢀⢁⢂⢃⢄⢅⢆⢇⣀⣁⣂⣃⣄⣅⣆⣇⢈⢉⢊⢋⢌⢍⢎⢏⣈⣉⣊⣋⣌⣍⣎⣏⢐⢑⢒⢓⢔⢕⢖⢗⣐⣑⣒⣓⣔⣕⣖⣗⢘⢙⢚⢛⢜⢝⢞⢟⣘⣙⣚⣛⣜⣝⣞⣟⢠⢡⢢⢣⢤⢥⢦⢧⣠⣡⣢⣣⣤⣥⣦⣧⢨⢩⢪⢫⢬⢭⢮⢯⣨⣩⣪⣫⣬⣭⣮⣯⢰⢱⢲⢳⢴⢵⢶⢷⣰⣱⣲⣳⣴⣵⣶⣷⢸⢹⢺⢻⢼⢽⢾⢿⣸⣹⣺⣻⣼⣽⣾⣿");

        const int bs[] = {0, 0x8, 0xc, 0xe, 0xf};
        //const int bs[] = {0, 0x8, 0x4, 0x2, 0x1};

        std::cout << '\r';

        const size_t resolution = width * 2;
        const size_t unitBarWidth = values.size() / resolution;
        for (size_t charIndex = 0; charIndex < width; ++charIndex)
        {
            const size_t leftIndex = unitBarWidth * (charIndex * 2 + 0);
            const size_t rightIndex = unitBarWidth * (charIndex * 2 + 1);

            int index = 0;
            {
                float maxValue = 0.0f;
                for (size_t i = leftIndex; i < leftIndex + unitBarWidth; ++i)
                {
                    maxValue = std::max(maxValue, values[i]);
                }

                const int xi = static_cast<int>(maxValue / 0.2f);
                const int x = std::max(0, std::min(4, xi));
                index |= bs[x];
            }

            {
                float maxValue = 0.0f;
                for (size_t i = rightIndex; i < rightIndex + unitBarWidth; ++i)
                {
                    maxValue = std::max(maxValue, values[i]*2);
                }

                const int xi = static_cast<int>(maxValue / 0.2f);
                const int x = std::max(0, std::min(4, xi));
                index |= (bs[x] << 4);
            }

            std::cout << str.substr(index*3, 3);
        }

        std::cout << std::flush;
    }

private:

    size_t width;
};

int main()
{
    SoundCapturer capturer;

    const size_t N = 4096;

    int samplingFrequency = 44100;
    SpectrumAnalyzer analyzer(N, samplingFrequency);

    Renderer renderer(32);

    if (!capturer.init(N * 2, samplingFrequency))
    {
        return 1;
    }

    for (;;)
    {
        capturer.update();

        analyzer.update(capturer.getBuffer(), capturer.bufferHeadIndex());

        renderer.draw(analyzer.spectrum());
    }

    return 0;
}
