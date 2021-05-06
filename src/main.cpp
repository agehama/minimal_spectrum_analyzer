#include <chrono>
#include <thread>

#include <cxxopts.hpp>

#include "SpectrumAnalyzer.hpp"
#include "Renderer.hpp"
#include "SoundCapturerPulseAudio.hpp"
#include "SoundCapturerWASAPI.hpp"

int main(int argc, const char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    int characterSize = 0;
    float bottomLevel = 0;
    float topLevel = 0;
    float minFreq = 0;
    float maxFreq = 0;
    int fftSize = 0;
    int zeroPaddingScale = 0;
    int windowSize = 0;
    float smoothing = 0;
    std::string lineFeed;
    try
    {
        cxxopts::Options options(argv[0], "A tiny, embeddable command-line sound visualizer");

        options.add_options()
            ("h,help", "print this message")
            ("c,chars", "draw the spectrum using N characters", cxxopts::value<int>()->default_value("32"), "N")
            ("b,bottom_db", "the minimum intensity(dB) of the spectrum to be displayed. N in [20, 40] is desirable", cxxopts::value<float>()->default_value("30"), "N")
            ("t,top_db", "the maximum intensity(dB) of the spectrum to be displayed. N in [50, 80] is desirable", cxxopts::value<float>()->default_value("70"), "N")
            ("l,lower_cutoff", "minimum cutoff frequency(Hz)", cxxopts::value<float>()->default_value("30"), "N")
            ("u,upper_cutoff", "maximum cutoff frequency(Hz)", cxxopts::value<float>()->default_value("5000"), "N")
            ("n,num_fft", "FFT sample size", cxxopts::value<int>()->default_value("8192"), "N")
            ("z,zero_padding", "zero padding rate", cxxopts::value<int>()->default_value("2"), "N")
            ("w,window_size", "gaussian smoothing window size", cxxopts::value<int>()->default_value("1"), "N")
            ("s,smoothing", "smoothing parameter", cxxopts::value<float>()->default_value("0.5"), "x in (0.0, 1.0]")
            ("line_feed", "line feed character", cxxopts::value<std::string>()->default_value("CR"), "{\'CR\'|\'LF\'|\'CRLF\'}")
            ;

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            return 0;
        }

        characterSize = result["chars"].as<int>();
        bottomLevel = result["bottom_db"].as<float>();
        topLevel = result["top_db"].as<float>();

        minFreq = result["lower_cutoff"].as<float>();
        maxFreq = result["upper_cutoff"].as<float>();

        fftSize = result["num_fft"].as<int>();
        zeroPaddingScale = result["zero_padding"].as<int>();
        windowSize = result["window_size"].as<int>();
        smoothing = result["smoothing"].as<float>();

        const std::string lineFeedStr = result["line_feed"].as<std::string>();
        if (lineFeedStr == "CR")
        {
            lineFeed = std::string("\r");
        }
        else if (lineFeedStr == "LF")
        {
            lineFeed = std::string("\n");
        }
        else if (lineFeedStr == "CRLF")
        {
            lineFeed = std::string("\r\n");
        }
        else
        {
            std::cerr << "error: invalid line feed character" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "error parsing options: " << e.what() << std::endl;
        return 1;
    }

#ifdef ANALYZER_USE_WASAPI
    SoundCapturerWASAPI capturer;
#else
    SoundCapturerPulseAudio capturer;
#endif

    int sampleSize = fftSize / zeroPaddingScale;
    int samplingFrequency = 48000;
    SpectrumAnalyzer analyzer(fftSize, zeroPaddingScale, samplingFrequency);

    Renderer renderer(characterSize, lineFeed);

    if (!capturer.init(sampleSize, samplingFrequency))
    {
        return 1;
    }

    const float maxFPS = 60.0f;

    const float millisecPerFrame = 1000.0f / maxFPS;

    for (int i = 0;;)
    {
        const auto t1 = std::chrono::high_resolution_clock::now();

        capturer.update();

        if (sampleSize < capturer.bufferReadCount())
        {
            analyzer.update(capturer.getBuffer(), capturer.bufferHeadIndex(), bottomLevel, topLevel, minFreq, maxFreq);

            renderer.draw(analyzer.spectrum(), windowSize, smoothing);

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
