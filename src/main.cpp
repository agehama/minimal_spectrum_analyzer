#include <cxxopts.hpp>

#include "SpectrumAnalyzer.hpp"
#include "SoundCapturer.hpp"
#include "Renderer.hpp"

int main(int argc, const char* argv[])
{
    try
    {
        cxxopts::Options options(argv[0], "A tiny sound visualizer running on the command line");

        options.add_options()
            ("h,help", "print this message")
            ("z,zero_padding", "zero padding scale (default=4)", cxxopts::value<int>(), "N")
            ("w,width", "number of characters to display (default=8)", cxxopts::value<int>(), "N")
            ("min_level", "minimum volume threshold for displayed spectrum (default=40)", cxxopts::value<float>(), "min")
            ("max_level", "maximum volume threshold for displayed spectrum (default=75)", cxxopts::value<float>(), "max")
            ;

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            return 0;
        }

        int zeroPadding = 4;
        if (result.count("zero_padding"))
        {
            zeroPadding = result["zero_padding"].as<int>();
        }
        std::cout << "zeroPadding: " << zeroPadding << std::endl;

        int width = 8;
        if (result.count("width"))
        {
            width = result["width"].as<int>();
        }
        std::cout << "width: " << width << std::endl;

        float minLevel = 40.0f;
        if (result.count("min_level"))
        {
            minLevel = result["min_level"].as<float>();
        }
        std::cout << "min_level: " << minLevel << std::endl;

        float maxLevel = 75.0f;
        if (result.count("max_level"))
        {
            maxLevel = result["max_level"].as<float>();
        }
        std::cout << "max_level: " << maxLevel << std::endl;

        SoundCapturer capturer;

        const size_t N = 4096;

        int samplingFrequency = 44100;
        SpectrumAnalyzer analyzer(N, zeroPadding, samplingFrequency);

        Renderer renderer(width);

        if (!capturer.init(N * 2, samplingFrequency))
        {
            return 1;
        }

        for (;;)
        {
            capturer.update();

            analyzer.update(capturer.getBuffer(), capturer.bufferHeadIndex(), minLevel, maxLevel);

            renderer.draw(analyzer.spectrum());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "error parsing options: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
