#include <vector>
#include <string>
#include <iostream>
#include <cstdio>

class Renderer
{
public:

    Renderer() = default;

    Renderer(size_t width)
        : width(width)
    {}

    void draw(const std::vector<float>& values)const
    {
        const std::string str("⠀⠁⠂⠃⠄⠅⠆⠇⡀⡁⡂⡃⡄⡅⡆⡇⠈⠉⠊⠋⠌⠍⠎⠏⡈⡉⡊⡋⡌⡍⡎⡏⠐⠑⠒⠓⠔⠕⠖⠗⡐⡑⡒⡓⡔⡕⡖⡗⠘⠙⠚⠛⠜⠝⠞⠟⡘⡙⡚⡛⡜⡝⡞⡟⠠⠡⠢⠣⠤⠥⠦⠧⡠⡡⡢⡣⡤⡥⡦⡧⠨⠩⠪⠫⠬⠭⠮⠯⡨⡩⡪⡫⡬⡭⡮⡯⠰⠱⠲⠳⠴⠵⠶⠷⡰⡱⡲⡳⡴⡵⡶⡷⠸⠹⠺⠻⠼⠽⠾⠿⡸⡹⡺⡻⡼⡽⡾⡿⢀⢁⢂⢃⢄⢅⢆⢇⣀⣁⣂⣃⣄⣅⣆⣇⢈⢉⢊⢋⢌⢍⢎⢏⣈⣉⣊⣋⣌⣍⣎⣏⢐⢑⢒⢓⢔⢕⢖⢗⣐⣑⣒⣓⣔⣕⣖⣗⢘⢙⢚⢛⢜⢝⢞⢟⣘⣙⣚⣛⣜⣝⣞⣟⢠⢡⢢⢣⢤⢥⢦⢧⣠⣡⣢⣣⣤⣥⣦⣧⢨⢩⢪⢫⢬⢭⢮⢯⣨⣩⣪⣫⣬⣭⣮⣯⢰⢱⢲⢳⢴⢵⢶⢷⣰⣱⣲⣳⣴⣵⣶⣷⢸⢹⢺⢻⢼⢽⢾⢿⣸⣹⣺⣻⣼⣽⣾⣿");

        const int bs[] = {0, 0x8, 0xc, 0xe, 0xf};
        //const int bs[] = {0, 0x8, 0x4, 0x2, 0x1};

        std::cout << '\r';
        std::cout << '|';

        const size_t resolution = width * 2;
        const size_t unitBarWidth = values.size() / resolution;
        for (size_t charIndex = 0; charIndex < width; ++charIndex)
        {
            const size_t leftIndex = unitBarWidth * (charIndex * 2 + 0);
            const size_t rightIndex = unitBarWidth * (charIndex * 2 + 1);

            int index = 0;
            {
                float maxValue = 0.0f;
                for (size_t i = leftIndex; i < leftIndex + unitBarWidth; ++i)
                {
                    maxValue = std::max(maxValue, values[i]);
                }

                const int xi = static_cast<int>(maxValue / 0.2f);
                const int x = std::max(0, std::min(4, xi));
                index |= bs[x];
            }

            {
                float maxValue = 0.0f;
                for (size_t i = rightIndex; i < rightIndex + unitBarWidth; ++i)
                {
                    maxValue = std::max(maxValue, values[i]*2);
                }

                const int xi = static_cast<int>(maxValue / 0.2f);
                const int x = std::max(0, std::min(4, xi));
                index |= (bs[x] << 4);
            }

            std::cout << str.substr(index*3, 3);
        }

        std::cout << '|';

        std::cout << std::flush;
    }

private:

    size_t width;
};