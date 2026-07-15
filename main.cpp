#include "app/TrackCamHubApp.h"
#include "app/Logger.h"
#include "app/ServiceRunner.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{

std::atomic<bool> g_console_stop_requested{false};

#ifdef _WIN32
BOOL WINAPI consoleControlHandler(DWORD control_type)
{
    switch (control_type)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_console_stop_requested.store(true);
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

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

    g_console_stop_requested.store(false);
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(consoleControlHandler, TRUE))
    {
        trackcamhub::Logger::warn("failed to register console control handler");
    }
#endif

    std::cout << "TrackCamHub is running. Press Ctrl+C to stop." << std::endl;
    while (!g_console_stop_requested.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

#ifdef _WIN32
    SetConsoleCtrlHandler(consoleControlHandler, FALSE);
#endif
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
