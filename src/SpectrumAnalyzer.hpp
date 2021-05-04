#include <vector>
#include <cstdint>
#include <cmath>
#include <cassert>

#include <fft.h>
#include <fft_internal.h>

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

    void update(const std::vector<float>& buffer, size_t headIndex, float dBsplMin, float dBsplMax)
    {
        assert(sampleSize == buffer.size());

        const float coef = 1.0f;
        for (size_t i = 0; i < sampleSize; ++i)
        {
            input[i] = coef * buffer[(headIndex + i) % buffer.size()];
        }

        executeFFT();

        updateSpectrum(dBsplMin, dBsplMax);
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

    void updateSpectrum(float dBsplMin, float dBsplMax)
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
