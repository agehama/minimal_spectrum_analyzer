#include "SpectrumAnalyzer.hpp"
#include "SoundCapturer.hpp"
#include "Renderer.hpp"

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
