#include <vector>
#include <cstdint>
#include <cmath>
#include <cassert>
#include <sstream>
#include <limits>

#include <fft.h>
#include <fft_internal.h>

class SpectrumAnalyzer
{
public:

    SpectrumAnalyzer() = default;

    SpectrumAnalyzer(size_t inputSampleSize, size_t fftSampleSize, int samplingFrequency)
    {
        init(inputSampleSize, fftSampleSize, samplingFrequency);
    }

    ~SpectrumAnalyzer()
    {
        mufft_free(input);
        mufft_free(input2);
        mufft_free(output);
        mufft_free_plan_1d(muplan);
    }

    void init(size_t inputSampleSize, size_t fftSampleSize, int samplingFrequency)
    {
        inputSampleSize = std::min(inputSampleSize, fftSampleSize);

        fftSize = fftSampleSize;
        inputSize = inputSampleSize;
        unitFreq = samplingFrequency / static_cast<float>(fftSize);
        sampleFreq = samplingFrequency;

        //std::cout << "inputSize: " << inputSize << std::endl;
        //std::cout << "fftSize: " << fftSize << std::endl;

        input = static_cast<float*>(mufft_alloc(inputSampleSize * sizeof(float)));
        input2 = static_cast<float*>(mufft_alloc(fftSize * sizeof(float)));
        output = static_cast<cfloat*>(mufft_alloc(fftSize * sizeof(cfloat)));
        muplan = mufft_create_plan_1d_r2c(fftSize, MUFFT_FLAG_CPU_ANY);

        initZeroLevel();
    }

    void update(const std::vector<float>& buffer, size_t headIndex, float minLevel, float maxLevel, float freqMin, float freqMax, float logBase)
    {
        if (buffer.size() < inputSize)
        {
            assert(buffer.size() < inputSize);
        }

        for (size_t i = 0; i < inputSize; ++i)
        {
            input[i] = buffer[(headIndex + i) % buffer.size()];
        }

        executeFFT();

        updateSpectrum(minLevel, maxLevel, freqMin, freqMax, logBase);
    }

    const std::vector<float>& spectrum()const
    {
        return spectrumView;
    }

    std::vector<std::pair<std::string, float>> getLabels(float freqMin, float freqMax, float logBase)const
    {
        std::vector<std::pair<std::string, float>> labels;

        const size_t outputSize = fftSize;

        const float logFreqMin = std::pow(freqMin, 1.0f / logBase);
        const float logFreqMax = std::pow(freqMax, 1.0f / logBase);

        const auto getLabelStr = [](int freq)
        {
            std::stringstream ss;

            if (freq < 1000)
            {
                ss << freq;
            }
            else if (0 == freq % 1000)
            {
                ss << (freq / 1000) << "k";
            }
            else
            {
                ss << (freq / 1000) << "." << (freq % 1000) / 100  << "k";
            }

            return ss.str();
        };

        labels.clear();
        labels.emplace_back(getLabelStr(freqMin), 0.0f);
        labels.emplace_back(getLabelStr(freqMax), 1.0f);

        // ordered in priority
        const std::vector<int> freqLabels({
            100, 1000, 10000, 50, 500, 5000,
            20, 200, 2000, 20000, 30, 300, 3000, 40, 400, 4000, 70, 700, 7000,
            60, 600, 6000, 80, 800, 8000, 90, 900, 9000,
            150, 1500, 15000, 550, 5500,
            250, 2500, 350, 3500, 450, 4500, 750, 7500,
            650, 6500, 850, 8500, 950, 9500,
            1100, 11000, 1200, 12000, 1300, 13000, 1400, 14000, 1600, 16000, 1700, 17000, 1800, 18000, 1900, 19000
        });

        for (size_t i = 0; i < freqLabels.size(); ++i)
        {
            const int freq = freqLabels[i];
            const float index = getAbscissa(freq, logBase, logFreqMin, logFreqMax);
            labels.emplace_back(getLabelStr(freq), index);
        }

        return labels;
    }

private:

    void initZeroLevel()
    {
        const int freq = 1000;
        const float pi = 3.1415926535f;
        for (size_t i = 0; i < inputSize; ++i)
        {
            const float t = 1.0f * i / sampleFreq;
            const float x = std::sin(freq * 2.0f * pi * t);
            input2[i] = x;
        }

        for (int i = inputSize; i < fftSize; ++i)
        {
            input2[i] = 0;
        }

        mufft_execute_plan_1d(muplan, output, input2);

        zeroLevel = std::numeric_limits<float>::lowest();
        for (size_t i = 1; i < fftSize; ++i)
        {
            const float pressureMax = getPower(i);

            const float f = unitFreq * i;

            //d weighting
            const float hf = ((1037918.48f - f * f) * (1037918.48f - f * f) + 1080768.16f * f * f) /
                ((9837328.0f - f * f) * (9837328.0f - f * f) + 11723776.0f * f * f);
            const float rd = (f / (6.8966888496476f * 1.0e-5f)) * std::sqrt(hf / ((f * f + 79919.29f) * (f * f + 1345600.0f)));
            const float spl = 10.0f * std::log10(rd * pressureMax);

            //const float spl = 10.0f * std::log10(pressureMax);

            zeroLevel = std::max(spl, zeroLevel);
        }
    }

    void executeFFT()
    {
        const float pi = 3.1415926535f;
        for (int i = 0; i < inputSize; ++i)
        {
            const float t = 1.0f * i / (inputSize - 1);
            const float hammingWindow = (0.54f - 0.46f * std::cos(2.0f * pi * t));
            input2[i] = hammingWindow * input[i];
        }

        for (int i = inputSize; i < fftSize; ++i)
        {
            input2[i] = 0;
        }

        mufft_execute_plan_1d(muplan, output, input2);
    }

    float getLogScaledFreq(float t, float logBase, float logFreqMin, float logFreqMax)const
    {
        const float logFreq = logFreqMin + (logFreqMax - logFreqMin) * t;
        const float freq = std::pow(logFreq, logBase);
        return freq;
    }

    float getAbscissa(float freq, float logBase, float logFreqMin, float logFreqMax)const
    {
        const float logFreq = std::pow(freq, 1.0f / logBase);
        return (logFreq - logFreqMin) / (logFreqMax - logFreqMin);
    }

    float getPower(size_t index)
    {
        const float normalizeCoef = 2.0f / fftSize;
        const cfloat a = output[index];
        float rx = a.real;
        float ix = a.imag;
        return normalizeCoef * std::sqrt(rx * rx + ix * ix);
    }

    void updateSpectrum(float minLevel, float maxLevel, float freqMin, float freqMax, float logBase)
    {
        const size_t outputSize = fftSize;
        const float normalizeCoef = 2.0f / outputSize;

        const float logFreqMin = std::pow(freqMin, 1.0f / logBase);
        const float logFreqMax = std::pow(freqMax, 1.0f / logBase);

        const float bottomLevel = zeroLevel + minLevel;
        const float topLevel = zeroLevel + maxLevel;

        spectrumView.resize(outputSize - 1);
        for (size_t i = 0; i < spectrumView.size(); ++i)
        {
            const float t0 = 1.0 * i / spectrumView.size();
            const float t1 = 1.0 * (i + 1) / spectrumView.size();

            const int index0 = static_cast<int>(std::floor(getLogScaledFreq(t0, logBase, logFreqMin, logFreqMax) / unitFreq));
            const int index1 = static_cast<int>(std::floor(getLogScaledFreq(t1, logBase, logFreqMin, logFreqMax) / unitFreq));

            float pressureMax = getPower(index0);
            for (int j = index0 + 1; j < index1; ++j)
            {
                //pressureMax = std::max(pressureMax, getPower(j));
                pressureMax += getPower(j);
            }

            const float f = unitFreq * (index0 + index1) * 0.5;

            //d weighting
            const float hf = ((1037918.48f - f * f) * (1037918.48f - f * f) + 1080768.16f * f * f) /
                ((9837328.0f - f * f) * (9837328.0f - f * f) + 11723776.0f * f * f);
            const float rd = (f / (6.8966888496476f * 1.0e-5f)) * std::sqrt(hf / ((f * f + 79919.29f) * (f * f + 1345600.0f)));
            const float spl = 10.0f * std::log10(rd * pressureMax);

            const float loudness = std::max(0.0f, (spl - bottomLevel)) / (topLevel - bottomLevel);

            spectrumView[i] = loudness;
        }
    }

    std::vector<float> spectrumView;

    float* input = nullptr;
    float* input2 = nullptr;
    cfloat* output = nullptr;
    mufft_plan_1d* muplan = nullptr;

    size_t fftSize = 0;
    size_t inputSize = 0;
    float unitFreq = 0.0f;
    float sampleFreq = 0.0f;
    float zeroLevel = 0.0f;
};
