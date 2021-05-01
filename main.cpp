#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

class SoundCapturer
{
public:

    SoundCapturer() = default;

    ~SoundCapturer()
    {
        if (streamServer)
        {
            pa_simple_free(streamServer);
        }
    }

    bool init()
    {
        const auto contextCallback = [](pa_context* context, void* userdata)
        {
            const auto callback = [](pa_context* context, const pa_server_info* info, void* userdata)
            {
                auto pData = reinterpret_cast<UserData*>(userdata);

                pData->sinkName = std::string(info->default_sink_name) + std::string(".monitor");

                pa_context_disconnect(context);
                pa_context_unref(context);
                pa_mainloop_quit(pData->mainloop, 0);
                pa_mainloop_free(pData->mainloop);
            };

            auto pData = reinterpret_cast<UserData*>(userdata);

            switch (pa_context_get_state(context))
            {
            case PA_CONTEXT_READY:
                pa_operation_unref(pa_context_get_server_info(context, callback, userdata));
                break;
            case PA_CONTEXT_FAILED:
                std::cerr << "error: PA_CONTEXT_FAILED\n";
                break;
            case PA_CONTEXT_TERMINATED:
                pa_mainloop_quit(pData->mainloop, 0);
                break;
            default: break;
            }
        };

        const std::string appName = std::string("minimal spectrum analyzer");

        UserData data;

        pa_mainloop_api* mainloop_api = pa_mainloop_get_api(data.mainloop);

        pa_context* pulseaudio_context = pa_context_new(mainloop_api, appName.c_str());

        pa_context_connect(pulseaudio_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

        pa_context_set_state_callback(pulseaudio_context, contextCallback, static_cast<void*>(&data));

        pa_mainloop_iterate(data.mainloop, 0, nullptr);

        pa_mainloop_run(data.mainloop, nullptr);

        const pa_sample_spec ss = {
            .format = PA_SAMPLE_S16LE,
            .rate = 44100,
            .channels = 2,
        };

        int error;
        streamServer = pa_simple_new(nullptr, appName.c_str(), PA_STREAM_RECORD, data.sinkName.c_str(), "loopback stream", &ss, nullptr, nullptr, &error);
        if (!streamServer)
        {
            std::cerr << "error: pa_simple_new() failed " << pa_strerror(error) << std::endl;
            return false;
        }

        buffer.resize(128);

        return true;
    }

    void update()
    {
        pa_simple_read(streamServer, buffer.data(), buffer.size()*sizeof(std::int16_t), nullptr);
        //std::cout << buffer[0] << ", " << buffer[1] << "\n";
    }

private:

    struct UserData
    {
        UserData()
            : mainloop(pa_mainloop_new())
        {}

        std::string sinkName;
        pa_mainloop* mainloop;
    };

    std::vector<std::int16_t> buffer;
    pa_simple* streamServer = nullptr;
};

class Renderer
{
public:

    Renderer() = default;

    Renderer(size_t width)
        : width(width)
    {}

    void draw(const std::vector<int>& xs)const
    {
        const std::string str("⠀⠁⠂⠃⠄⠅⠆⠇⡀⡁⡂⡃⡄⡅⡆⡇⠈⠉⠊⠋⠌⠍⠎⠏⡈⡉⡊⡋⡌⡍⡎⡏⠐⠑⠒⠓⠔⠕⠖⠗⡐⡑⡒⡓⡔⡕⡖⡗⠘⠙⠚⠛⠜⠝⠞⠟⡘⡙⡚⡛⡜⡝⡞⡟⠠⠡⠢⠣⠤⠥⠦⠧⡠⡡⡢⡣⡤⡥⡦⡧⠨⠩⠪⠫⠬⠭⠮⠯⡨⡩⡪⡫⡬⡭⡮⡯⠰⠱⠲⠳⠴⠵⠶⠷⡰⡱⡲⡳⡴⡵⡶⡷⠸⠹⠺⠻⠼⠽⠾⠿⡸⡹⡺⡻⡼⡽⡾⡿⢀⢁⢂⢃⢄⢅⢆⢇⣀⣁⣂⣃⣄⣅⣆⣇⢈⢉⢊⢋⢌⢍⢎⢏⣈⣉⣊⣋⣌⣍⣎⣏⢐⢑⢒⢓⢔⢕⢖⢗⣐⣑⣒⣓⣔⣕⣖⣗⢘⢙⢚⢛⢜⢝⢞⢟⣘⣙⣚⣛⣜⣝⣞⣟⢠⢡⢢⢣⢤⢥⢦⢧⣠⣡⣢⣣⣤⣥⣦⣧⢨⢩⢪⢫⢬⢭⢮⢯⣨⣩⣪⣫⣬⣭⣮⣯⢰⢱⢲⢳⢴⢵⢶⢷⣰⣱⣲⣳⣴⣵⣶⣷⢸⢹⢺⢻⢼⢽⢾⢿⣸⣹⣺⣻⣼⣽⣾⣿");

        const int bs[] = {0, 0x8, 0xc, 0xe, 0xf};
        //const int bs[] = {0, 0x8, 0x4, 0x2, 0x1};

        std::cout << '\r';

        for (int i = 0; i < xs.size(); i += 2)
        {
            const int x = std::max(0, std::min(4, xs[i]));
            int index = bs[x];
            if (i + 1 < xs.size())
            {
                const int x1 = std::max(0, std::min(4, xs[i + 1]));
                index |= (bs[x1] << 4);
            }

            std::cout << str.substr(index*3, 3);
        }

        std::cout << std::flush;
    }

private:

    size_t width;
};

int main()
{
    SoundCapturer capturer;

    std::vector<int> xs({0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 0});
    Renderer renderer(xs.size());

    if (!capturer.init())
    {
        return 1;
    }

    for (;;)
    {
        capturer.update();

        renderer.draw(xs);
        std::rotate(xs.begin(), xs.begin() + 1, xs.end());
    }

    return 0;
}
