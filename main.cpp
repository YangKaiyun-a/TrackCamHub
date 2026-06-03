#include "app/TrackCamHubApp.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
    const std::string config_path = argc > 1 ? argv[1] : "config/trackcamhub.ini";

    try
    {
        trackcamhub::TrackCamHubApp app;
        if (!app.start(config_path))
        {
            return 1;
        }

        std::cout << "TrackCamHub is running. Press Enter to stop..." << std::endl;
        std::cin.get();
        app.stop();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "TrackCamHub fatal error: " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "TrackCamHub fatal error: unknown exception" << std::endl;
    }

    return 1;
}
