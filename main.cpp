#include "app/TrackCamHubApp.h"
#include "app/Logger.h"
#include "app/ServiceRunner.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

std::filesystem::path executableDirectory(char* argv0)
{
    std::filesystem::path executable_path = argv0 ? std::filesystem::path(argv0) : std::filesystem::path();
    if (executable_path.is_relative())
    {
        executable_path = std::filesystem::current_path() / executable_path;
    }
    return executable_path.parent_path();
}

void printUsage()
{
    std::cout << "Usage:\n"
              << "  TrackCamHub.exe --console [config_path]\n"
              << "  TrackCamHub.exe --service [config_path]\n"
              << "  TrackCamHub.exe [config_path]\n";
}

int runConsole(const std::string& config_path)
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

} // namespace

int main(int argc, char* argv[])
{
    const auto exe_dir = executableDirectory(argc > 0 ? argv[0] : nullptr);
    trackcamhub::Logger::setLogFile((exe_dir / "logs" / "TrackCamHub.log").string());

    std::string mode = "--console";
    std::string config_path = (exe_dir / "config" / "trackcamhub.ini").string();

    if (argc > 1)
    {
        const std::string first_arg = argv[1];
        if (first_arg == "--help" || first_arg == "-h" || first_arg == "/?")
        {
            printUsage();
            return 0;
        }

        if (first_arg == "--console" || first_arg == "--service")
        {
            mode = first_arg;
            if (argc > 2)
            {
                config_path = argv[2];
            }
        }
        else
        {
            config_path = first_arg;
        }
    }

    try
    {
        if (mode == "--service")
        {
            return trackcamhub::ServiceRunner::run(config_path);
        }

        return runConsole(config_path);
    }
    catch (const std::exception& ex)
    {
        trackcamhub::Logger::error("TrackCamHub fatal error: " + std::string(ex.what()));
        std::cerr << "TrackCamHub fatal error: " << ex.what() << std::endl;
    }
    catch (...)
    {
        trackcamhub::Logger::error("TrackCamHub fatal error: unknown exception");
        std::cerr << "TrackCamHub fatal error: unknown exception" << std::endl;
    }

    return 1;
}
