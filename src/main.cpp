#include <cxxopts.hpp>

#include "SpectrumAnalyzer.hpp"
#include "Renderer.hpp"
#include "SoundCapturerPulseAudio.hpp"
#include "SoundCapturerWASAPI.hpp"

int main(int argc, const char* argv[])
{
#ifdef _WINDOWS
    SetConsoleOutputCP(CP_UTF8);
#endif

    int zeroPadding = 0;
    int width = 0;
    float minLevel = 0;
    float maxLevel = 0;
    std::string lineFeed;

    try
    {
        cxxopts::Options options(argv[0], "A tiny sound visualizer running on the command line");

        options.add_options()
            ("h,help", "print this message")
            ("z,zero_padding", "zero padding scale", cxxopts::value<int>()->default_value("4"), "N")
            ("w,width", "use N characters to draw the spectrum", cxxopts::value<int>()->default_value("16"), "N")
            ("b,bottom_db", "the minimum intensity of the spectrum to be displayed. N in [20, 40] is desirable", cxxopts::value<float>()->default_value("30"), "N")
            ("t,top_db", "the maximum intensity of the spectrum to be displayed. N in [60, 80] is desirable", cxxopts::value<float>()->default_value("75"), "N")
            ("line_feed", "line feed character", cxxopts::value<std::string>()->default_value("CR"), "{\'CR\'|\'LF\'|\'CRLF\'}")
            ;

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            return 0;
        }

        zeroPadding = result["zero_padding"].as<int>();

        width = result["width"].as<int>();

        minLevel = result["bottom_db"].as<float>();
        maxLevel = result["top_db"].as<float>();

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

    const size_t N = 8192;

    int samplingFrequency = 48000;
    SpectrumAnalyzer analyzer(N, zeroPadding, samplingFrequency);

    Renderer renderer(width, lineFeed);

    if (!capturer.init(N, samplingFrequency))
    {
        return 1;
    }

    for (int i = 0;;)
    {
        capturer.update();

        if (N < capturer.bufferReadCount())
        {
            analyzer.update(capturer.getBuffer(), capturer.bufferHeadIndex(), minLevel, maxLevel);

            renderer.draw(analyzer.spectrum());

            ++i;
        }
    }

    return 0;
}
