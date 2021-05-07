#include <chrono>
#include <thread>

#include <cxxopts.hpp>

#include "SpectrumAnalyzer.hpp"
#include "Renderer.hpp"
#include "Axis.hpp"
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
    int inputSize = 0;
    int windowSize = 0;
    float smoothing = 0;
    bool displayAxis = false;
    std::string lineFeed;
    try
    {
        cxxopts::Options options(argv[0], "A tiny, embeddable command-line sound visualizer");

        options.add_options()
            ("h,help", "print this message")
            ("c,chars", "draw the spectrum using N characters", cxxopts::value<int>()->default_value("32"), "N")
            ("t,top_db", "the maximum intensity(dB) of the spectrum to be displayed.", cxxopts::value<float>()->default_value("-6"), "x")
            ("b,bottom_db", "the minimum intensity(dB) of the spectrum to be displayed.", cxxopts::value<float>()->default_value("-30"), "x")
            ("l,lower_freq", "minimum cutoff frequency(Hz)", cxxopts::value<float>()->default_value("30"), "x")
            ("u,upper_freq", "maximum cutoff frequency(Hz)", cxxopts::value<float>()->default_value("5000"), "x")
            ("f,fft_size", "FFT sample size", cxxopts::value<int>()->default_value("8192"), "N")
            ("i,input_size", "input sample size. input_size must be less or equal to the fft_size.", cxxopts::value<int>()->default_value("2048"), "N")
            ("g,gaussian_diameter", "display each spectrum bar with a Gaussian blur with the surrounding N bars.", cxxopts::value<int>()->default_value("1"), "N")
            ("s,smoothing", "linear interpolation parameter for the previous frame. if 1.0, always display the latest value.", cxxopts::value<float>()->default_value("0.5"), "x in (0.0, 1.0]")
            ("a,axis", "display axis if 'on'", cxxopts::value<std::string>()->default_value("on"), "{\'on\'|\'off\'}")
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

        minFreq = result["lower_freq"].as<float>();
        maxFreq = result["upper_freq"].as<float>();

        fftSize = result["fft_size"].as<int>();
        inputSize = result["input_size"].as<int>();

        windowSize = result["gaussian_diameter"].as<int>();
        smoothing = result["smoothing"].as<float>();

        const std::string axisStr = result["axis"].as<std::string>();
        if (axisStr == "on")
        {
            displayAxis = true;
        }
        else if (axisStr == "off")
        {
            displayAxis = false;
        }
        else
        {
            std::cerr << "error: --axis \'" << axisStr  << "\'" << " is invalid parameter" << std::endl;
            return 1;
        }

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
            std::cerr << "error: --line_feed \'" << lineFeedStr  << "\'" << " is invalid parameter" << std::endl;
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

    int samplingFrequency = 48000;
    SpectrumAnalyzer analyzer(inputSize, fftSize, samplingFrequency);

    if (displayAxis)
    {
        Axis::PrintAxis(characterSize, analyzer.getLabels(minFreq, maxFreq));

        std::cout << "_/> " << topLevel << " [dB]\n";
    }

    Renderer renderer(characterSize, lineFeed);

    if (!capturer.init(inputSize, samplingFrequency))
    {
        return 1;
    }

    const float maxFPS = 60.0f;

    const float millisecPerFrame = 1000.0f / maxFPS;

    for (int i = 0;;)
    {
        const auto t1 = std::chrono::high_resolution_clock::now();

        capturer.update();

        if (inputSize < capturer.bufferReadCount())
        {
            analyzer.update(capturer.getBuffer(), capturer.bufferHeadIndex(), bottomLevel, topLevel, minFreq, maxFreq);

            renderer.draw(analyzer.spectrum(), windowSize, smoothing, displayAxis);

            if (displayAxis)
            {
                std::cout << "_/> " << bottomLevel << " [dB]";
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
