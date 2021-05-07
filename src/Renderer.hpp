#include <vector>
#include <numeric>
#include <string>
#include <iostream>
#include <cstdio>

class Renderer
{
public:

    Renderer() = default;

    Renderer(size_t width, const std::string& lineFeed)
        : width(width)
        , lineFeed(lineFeed)
    {}

    void draw(const std::vector<float>& values, int windowSize, float smoothing, bool displayAxis)
    {
        const std::string str("⠀⠁⠂⠃⠄⠅⠆⠇⡀⡁⡂⡃⡄⡅⡆⡇⠈⠉⠊⠋⠌⠍⠎⠏⡈⡉⡊⡋⡌⡍⡎⡏⠐⠑⠒⠓⠔⠕⠖⠗⡐⡑⡒⡓⡔⡕⡖⡗⠘⠙⠚⠛⠜⠝⠞⠟⡘⡙⡚⡛⡜⡝⡞⡟⠠⠡⠢⠣⠤⠥⠦⠧⡠⡡⡢⡣⡤⡥⡦⡧⠨⠩⠪⠫⠬⠭⠮⠯⡨⡩⡪⡫⡬⡭⡮⡯⠰⠱⠲⠳⠴⠵⠶⠷⡰⡱⡲⡳⡴⡵⡶⡷⠸⠹⠺⠻⠼⠽⠾⠿⡸⡹⡺⡻⡼⡽⡾⡿⢀⢁⢂⢃⢄⢅⢆⢇⣀⣁⣂⣃⣄⣅⣆⣇⢈⢉⢊⢋⢌⢍⢎⢏⣈⣉⣊⣋⣌⣍⣎⣏⢐⢑⢒⢓⢔⢕⢖⢗⣐⣑⣒⣓⣔⣕⣖⣗⢘⢙⢚⢛⢜⢝⢞⢟⣘⣙⣚⣛⣜⣝⣞⣟⢠⢡⢢⢣⢤⢥⢦⢧⣠⣡⣢⣣⣤⣥⣦⣧⢨⢩⢪⢫⢬⢭⢮⢯⣨⣩⣪⣫⣬⣭⣮⣯⢰⢱⢲⢳⢴⢵⢶⢷⣰⣱⣲⣳⣴⣵⣶⣷⢸⢹⢺⢻⢼⢽⢾⢿⣸⣹⣺⣻⣼⣽⣾⣿");

        const int bs[] = {0, 0x8, 0xc, 0xe, 0xf};
        //const int bs[] = {0, 0x8, 0x4, 0x2, 0x1};

        if (!isFirst)
        {
            std::cout << lineFeed;
        }
        isFirst = false;

        if (displayAxis)
        {
            std::cout << "│";
        }

        const size_t resolution = width * 2;
        const size_t unitBarWidth = values.size() / resolution;

        buffer1.resize(resolution);
        for (size_t barIndex = 0; barIndex < resolution; ++barIndex)
        {
            const size_t beginIndex = unitBarWidth * barIndex;
            const size_t endIndex = unitBarWidth * (barIndex + 1);

            float maxValue = 0.0f;
            for (size_t i = beginIndex; i < endIndex; ++i)
            {
                maxValue = std::max(maxValue, values[i]);
            }

            buffer1[barIndex] += (maxValue - buffer1[barIndex]) * smoothing;
        }

        const auto gaussianWeights = [](int windowSize, float variance)
        {
            const float pi = 3.1415926535f;
            std::vector<float> result(windowSize);
            int centerIndex = windowSize / 2;
            for (int i = 0; i < windowSize; ++i)
            {
                int currentX = i - centerIndex;
                result[i] = (1.0f / std::sqrt(2.0f * pi * variance)) * std::exp(-currentX * currentX / (2.0f * variance));
            }

            float sum = std::accumulate(result.begin(), result.end(), 0.0f);
            for(int i = 0; i < windowSize; ++i)
            {
                result[i] /= sum;
            }

            return result;
        };

        buffer2.resize(buffer1.size());
        const auto weights = gaussianWeights(windowSize, 1.0f);
        for (size_t barIndex = 0; barIndex < resolution; ++barIndex)
        {
            float value = 0.0f;
            for (int i = 0; i < windowSize; ++i)
            {
                int offset = i - windowSize / 2;
                value += (0 <= barIndex + offset && barIndex + offset < resolution ? buffer1[barIndex + offset] : 0.0f) * weights[i];
            }

            buffer2[barIndex] = value;
        }

        for (size_t charIndex = 0; charIndex < width; ++charIndex)
        {
            const size_t leftIndex = unitBarWidth * (charIndex * 2 + 0);
            const size_t rightIndex = unitBarWidth * (charIndex * 2 + 1);

            int index = 0;

            {
                const float v = buffer2[charIndex*2 + 0];

                const int xi = static_cast<int>(v / 0.2f);
                const int x = std::max(0, std::min(4, xi));
                index |= bs[x];
            }

            {
                const float v = buffer2[charIndex*2 + 1];

                const int xi = static_cast<int>(v / 0.2f);
                const int x = std::max(0, std::min(4, xi));
                index |= (bs[x] << 4);
            }

            std::cout << str.substr(index*3, 3);
        }

        if (displayAxis)
        {
            std::cout << "│";
        }
    }

private:

    std::vector<float> buffer1;
    std::vector<float> buffer2;
    std::string lineFeed;
    size_t width;
    bool isFirst = true;
};
