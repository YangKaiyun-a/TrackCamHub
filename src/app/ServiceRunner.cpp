#include "app/ServiceRunner.h"

#include "app/Logger.h"
#include "app/TrackCamHubApp.h"

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace trackcamhub
{

#ifdef _WIN32
namespace
{

constexpr const wchar_t* kServiceName = L"TrackCamHub";

std::mutex g_mutex;
std::condition_variable g_stop_cv;
bool g_stop_requested = false;
std::string g_config_path;
SERVICE_STATUS_HANDLE g_status_handle = nullptr;
SERVICE_STATUS g_status{};
std::unique_ptr<TrackCamHubApp> g_app;

void setServiceStatus(DWORD state, DWORD exit_code = NO_ERROR, DWORD wait_hint = 0)
{
    if (!g_status_handle)
    {
        return;
    }

    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = exit_code;
    g_status.dwWaitHint = wait_hint;
    g_status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;

    SetServiceStatus(g_status_handle, &g_status);
}

DWORD WINAPI serviceControlHandler(DWORD control, DWORD, LPVOID, LPVOID)
{
    if (control != SERVICE_CONTROL_STOP && control != SERVICE_CONTROL_SHUTDOWN)
    {
        return NO_ERROR;
    }

    setServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_stop_requested = true;
    }
    g_stop_cv.notify_all();

    if (g_app)
    {
        g_app->stop();
    }

    return NO_ERROR;
}

void WINAPI serviceMain(DWORD, LPWSTR*)
{
    g_status_handle = RegisterServiceCtrlHandlerExW(kServiceName, serviceControlHandler, nullptr);
    if (!g_status_handle)
    {
        return;
    }

    setServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    try
    {
        g_app = std::make_unique<TrackCamHubApp>();
        if (!g_app->start(g_config_path))
        {
            Logger::error("TrackCamHub service start failed.");
            setServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
            return;
        }

        Logger::info("TrackCamHub service started.");
        setServiceStatus(SERVICE_RUNNING);

        std::unique_lock<std::mutex> lock(g_mutex);
        g_stop_cv.wait(lock, [] { return g_stop_requested; });

        lock.unlock();
        if (g_app)
        {
            g_app->stop();
            g_app.reset();
        }

        Logger::info("TrackCamHub service stopped.");
        setServiceStatus(SERVICE_STOPPED);
    }
    catch (const std::exception& ex)
    {
        Logger::error("TrackCamHub service fatal error: " + std::string(ex.what()));
        setServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
    }
    catch (...)
    {
        Logger::error("TrackCamHub service fatal error: unknown exception");
        setServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
    }
}

} // namespace
#endif

int ServiceRunner::run(const std::string& config_path)
{
#ifdef _WIN32
    g_config_path = config_path;
    g_stop_requested = false;

    SERVICE_TABLE_ENTRYW service_table[] = {
        {const_cast<LPWSTR>(kServiceName), serviceMain},
        {nullptr, nullptr},
    };

    if (!StartServiceCtrlDispatcherW(service_table))
    {
        const DWORD error = GetLastError();
        Logger::error("StartServiceCtrlDispatcher failed: " + std::to_string(error));
        return 1;
    }
    return 0;
#else
    (void)config_path;
    Logger::error("Windows service mode is only available on Windows.");
    return 1;
#endif
}

} // namespace trackcamhub
