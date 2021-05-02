#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include <fft.h>
#include <fft_internal.h>

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

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
        mufft_free(output);
        mufft_free_plan_1d(muplan);
    }

    void init(size_t fftSampleSize, int samplingFrequency)
    {
        sampleSize = fftSampleSize;
        unitFreq = samplingFrequency / static_cast<float>(sampleSize);

        input = static_cast<float*>(mufft_alloc(sampleSize * sizeof(float)));
        output = static_cast<cfloat*>(mufft_alloc(sampleSize * sizeof(cfloat)));
        muplan = mufft_create_plan_1d_r2c(sampleSize, MUFFT_FLAG_CPU_ANY);
    }

    void update(const std::vector<std::int16_t>& buffer)
    {
        assert(sampleSize * 2 == buffer.size());

        const float coef = 1.0f / 32768.0f;
        for (size_t i = 0; i < sampleSize; ++i)
        {
            input[i] = coef * buffer[i*2];
        }

        mufft_execute_plan_1d(muplan, output, input);

        const size_t outputSize = sampleSize / 2;
        const float normalizeCoef = 2.0f / outputSize;

        spectrumView.resize(outputSize - 1);
        for (size_t i = 0; i < spectrumView.size(); ++i)
        {
            const cfloat x = output[i + 1];
            const float rx = normalizeCoef * x.real;
            const float ix = normalizeCoef * x.imag;

            spectrumView[i] = std::sqrt(rx*rx + ix*ix);
        }
    }

    const std::vector<float>& spectrum()const
    {
        return spectrumView;
    }

private:

    std::vector<float> spectrumView;

    float* input = nullptr;
    cfloat* output = nullptr;
    mufft_plan_1d* muplan = nullptr;

    size_t sampleSize = 0;
    float unitFreq = 0.0f;
};

class SoundCapturer
{
public:

    SoundCapturer() = default;

    ~SoundCapturer()
    {
        if (streamServer)
        {
            pa_simple_free(streamServer);
        }
    }

    bool init(size_t bufferSize, int samplingFrequency)
    {
        const auto contextCallback = [](pa_context* context, void* userdata)
        {
            const auto callback = [](pa_context* context, const pa_server_info* info, void* userdata)
            {
                auto pData = reinterpret_cast<UserData*>(userdata);

                pData->sinkName = std::string(info->default_sink_name) + std::string(".monitor");

                pa_context_disconnect(context);
                pa_context_unref(context);
                pa_mainloop_quit(pData->mainloop, 0);
                pa_mainloop_free(pData->mainloop);
            };

            auto pData = reinterpret_cast<UserData*>(userdata);

            switch (pa_context_get_state(context))
            {
            case PA_CONTEXT_READY:
                pa_operation_unref(pa_context_get_server_info(context, callback, userdata));
                break;
            case PA_CONTEXT_FAILED:
                std::cerr << "error: PA_CONTEXT_FAILED\n";
                break;
            case PA_CONTEXT_TERMINATED:
                pa_mainloop_quit(pData->mainloop, 0);
                break;
            default: break;
            }
        };

        const std::string appName = std::string("minimal spectrum analyzer");

        UserData data;

        pa_mainloop_api* mainloop_api = pa_mainloop_get_api(data.mainloop);

        pa_context* pulseaudio_context = pa_context_new(mainloop_api, appName.c_str());

        pa_context_connect(pulseaudio_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

        pa_context_set_state_callback(pulseaudio_context, contextCallback, static_cast<void*>(&data));

        pa_mainloop_iterate(data.mainloop, 0, nullptr);

        pa_mainloop_run(data.mainloop, nullptr);

        const pa_sample_spec ss = {
            .format = PA_SAMPLE_S16LE,
            .rate = static_cast<std::uint32_t>(samplingFrequency),
            .channels = 2,
        };

        int error;
        streamServer = pa_simple_new(nullptr, appName.c_str(), PA_STREAM_RECORD, data.sinkName.c_str(), "loopback stream", &ss, nullptr, nullptr, &error);
        if (!streamServer)
        {
            std::cerr << "error: pa_simple_new() failed " << pa_strerror(error) << std::endl;
            return false;
        }

        buffer.resize(bufferSize);

        return true;
    }

    void update()
    {
        pa_simple_read(streamServer, buffer.data(), buffer.size() * sizeof(std::int16_t), nullptr);
        //std::cout << buffer[0] << ", " << buffer[1] << "\n";
    }

    const std::vector<std::int16_t>& getBuffer()const
    {
        return buffer;
    }

private:

    struct UserData
    {
        UserData()
            : mainloop(pa_mainloop_new())
        {}

        std::string sinkName;
        pa_mainloop* mainloop;
    };

    std::vector<std::int16_t> buffer;
    pa_simple* streamServer = nullptr;
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
                    maxValue = std::max(maxValue, values[i]*2);
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

    const size_t N = 2048;

    int samplingFrequency = 44100;
    SpectrumAnalyzer analyzer(N, samplingFrequency);

    Renderer renderer(16);

    if (!capturer.init(N * 2, samplingFrequency))
    {
        return 1;
    }

    for (;;)
    {
        capturer.update();

        analyzer.update(capturer.getBuffer());

        renderer.draw(analyzer.spectrum());
    }

    return 0;
}
