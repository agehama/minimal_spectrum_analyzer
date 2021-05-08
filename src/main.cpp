#include <chrono>
#include <thread>

#include "SpectrumAnalyzer.hpp"
#include "Renderer.hpp"
#include "Axis.hpp"
#include "Option.hpp"
#include "SoundCapturerPulseAudio.hpp"
#include "SoundCapturerWASAPI.hpp"

int main(int argc, const char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Option option;
    const bool suceeded = option.init(argc, argv);
    if (!option.isInitialized())
    {
        return suceeded ? 0 : 1;
    }

#ifdef ANALYZER_USE_WASAPI
    SoundCapturerWASAPI capturer;
#else
    SoundCapturerPulseAudio capturer;
#endif

    int samplingFrequency = 48000;
    SpectrumAnalyzer analyzer(option.inputSize, option.fftSize, samplingFrequency);

    if (option.displayAxis)
    {
        Axis::PrintAxis(option.characterSize, analyzer.getLabels(option.minFreq, option.maxFreq));

        std::cout << "_/> " << option.topLevel << " [dB]\n";
    }

    Renderer renderer(option.characterSize, option.lineFeed);

    if (!capturer.init(option.inputSize, samplingFrequency))
    {
        return 1;
    }

    const float maxFPS = 60.0f;

    const float millisecPerFrame = 1000.0f / maxFPS;

    for (int i = 0;;)
    {
        const auto t1 = std::chrono::high_resolution_clock::now();

        capturer.update();

        if (option.inputSize < capturer.bufferReadCount())
        {
            analyzer.update(capturer.getBuffer(), capturer.bufferHeadIndex(), option.bottomLevel, option.topLevel, option.minFreq, option.maxFreq);

            renderer.draw(analyzer.spectrum(), option.windowSize, option.smoothing, option.displayAxis);

            if (option.displayAxis)
            {
                std::cout << "_/> " << option.bottomLevel << " [dB]";
            }

            std::cout << std::flush;

            const auto t2 = std::chrono::high_resolution_clock::now();

            std::chrono::duration<float, std::milli> elapsed = t2 - t1;

            if (elapsed.count() < millisecPerFrame)
            {
                const int millisecSleep = static_cast<int>(millisecPerFrame - elapsed.count());
                std::this_thread::sleep_for(std::chrono::milliseconds(millisecSleep));
            }

            ++i;
        }
    }

    return 0;
}
