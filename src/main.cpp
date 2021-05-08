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
            ("h,help", "print this message.")
            ("c,chars", "draw the spectrum using N characters.", cxxopts::value<int>()->default_value("32"), "N")
            ("t,top_db", "the maximum intensity(dB) of the spectrum to be displayed.", cxxopts::value<float>()->default_value("-6"), "x")
            ("b,bottom_db", "the minimum intensity(dB) of the spectrum to be displayed.", cxxopts::value<float>()->default_value("-30"), "x")
            ("l,lower_freq", "minimum cutoff frequency(Hz).", cxxopts::value<float>()->default_value("30"), "x")
            ("u,upper_freq", "maximum cutoff frequency(Hz).", cxxopts::value<float>()->default_value("5000"), "x")
            ("f,fft_size", "FFT sample size. N must be power of two.", cxxopts::value<int>()->default_value("8192"), "N")
            ("i,input_size", "N <= fft_size is input sample size.", cxxopts::value<int>()->default_value("2048"), "N")
            ("g,gaussian_diameter", "display each spectrum bar with a Gaussian blur with the surrounding N bars.", cxxopts::value<int>()->default_value("1"), "N")
            ("s,smoothing", "x in (0.0, 1.0] is linear interpolation parameter for the previous frame. if 1.0, always display the latest value.", cxxopts::value<float>()->default_value("0.5"), "x")
            ("a,axis", "display axis if 'on'.", cxxopts::value<std::string>()->default_value("on"), "{\'on\'|\'off\'}")
            ("line_feed", "line feed character.", cxxopts::value<std::string>()->default_value("CR"), "{\'CR\'|\'LF\'|\'CRLF\'}")
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
        if (0.0f < bottomLevel)
        {
            std::cerr << "error: --bottom_db \'" << bottomLevel  << "\'" << " is invalid parameter." << std::endl;
            std::cerr << "       bottom_db should be smaller than 0.\n";
            return 1;
        }
        if (0.0f < topLevel)
        {
            std::cerr << "error: --top_db \'" << topLevel  << "\'" << " is invalid parameter." << std::endl;
            std::cerr << "       top_db should be smaller than 0.\n";
            return 1;
        }

        minFreq = result["lower_freq"].as<float>();
        maxFreq = result["upper_freq"].as<float>();

        fftSize = result["fft_size"].as<int>();
        const int expMin = 4;
        const int expMax = 17;
        std::vector<int> nList(expMax - expMin);
        for (size_t i = 0; i < nList.size(); ++i)
        {
            nList[i] = 1 << (expMin + i);
        }

        if (std::none_of(nList.begin(), nList.end(), [fftSize](int n){ return n == fftSize; }))
        {
            std::cerr << "error: --fft_size \'" << fftSize  << "\'" << " is invalid parameter." << std::endl;
            std::cerr << "       fft_size must be one of {";
            for (size_t i = 0; i < nList.size(); ++i)
            {
                std::cerr << nList[i];
                if (i + 1 != nList.size())
                {
                    std::cerr << ", ";
                }
            }
            std::cerr << "}.\n";
            return 1;
        }

        inputSize = result["input_size"].as<int>();
        if (fftSize < inputSize)
        {
            std::cerr << "error: --input_size \'" << inputSize  << "\'" << " is invalid parameter." << std::endl;
            std::cerr << "       input_size must be less or equal to the fft_size=" << fftSize << ".\n";
            return 1;
        }

        windowSize = result["gaussian_diameter"].as<int>();
        smoothing = result["smoothing"].as<float>();

        std::string axisStr = result["axis"].as<std::string>();
        std::transform(axisStr.begin(), axisStr.end(), axisStr.begin(), tolower);
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
            std::cerr << "error: --axis \'" << axisStr  << "\'" << " is invalid parameter." << std::endl;
            std::cerr << "       axis must be either 'on' or 'off'.\n";
            return 1;
        }

        std::string lineFeedStr = result["line_feed"].as<std::string>();
        std::transform(lineFeedStr.begin(), lineFeedStr.end(), lineFeedStr.begin(), toupper);
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
            std::cerr << "error: --line_feed \'" << lineFeedStr << "\'" << " is invalid parameter." << std::endl;
            std::cerr << "       line_feed must be either 'CR', 'LF' or 'CRLF'.\n";
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
