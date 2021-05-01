#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include <vector>
#include <string>
#include <iostream>

int main()
{
    struct UserData
    {
        UserData()
            : mainloop(pa_mainloop_new())
        {}

        std::string sinkName;
        pa_mainloop* mainloop;
    };

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
    pa_simple *s = pa_simple_new(nullptr, appName.c_str(), PA_STREAM_RECORD, data.sinkName.c_str(), "loopback stream", &ss, nullptr, nullptr, &error);
    if (!s)
    {
        std::cerr << "error: pa_simple_new() failed " << pa_strerror(error) << std::endl;
        return 1;
    }

    std::vector<std::int16_t> buffer(128);
    for (;;)
    {
        pa_simple_read(s, buffer.data(), buffer.size()*sizeof(std::int16_t), nullptr);

        std::cout << buffer[0] << ", " << buffer[1] << "\n";
    }

    pa_simple_free(s);

    return 0;
}
