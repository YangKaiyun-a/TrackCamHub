#include "app/TrackCamHubApp.h"

#include "app/Logger.h"
#include "config/AppConfig.h"

#include <filesystem>

namespace trackcamhub
{
namespace
{

std::filesystem::path appRootFromConfigPath(const std::string& config_path)
{
    std::filesystem::path path(config_path);
    if (path.is_relative())
    {
        path = std::filesystem::current_path() / path;
    }

    const auto config_dir = path.parent_path();
    if (config_dir.filename() == "config")
    {
        return config_dir.parent_path();
    }

    return config_dir.empty() ? std::filesystem::current_path() : config_dir;
}

} // namespace

TrackCamHubApp::~TrackCamHubApp()
{
    stop();
}

bool TrackCamHubApp::start(const std::string& config_path)
{
    if (running_.load())
    {
        return true;
    }

    config_ = AppConfigLoader::load(config_path);
    Logger::info("loaded config: " + config_path);

    camera_client_.configure(config_.camera);
    workflow_.configure(config_.camera, &camera_client_);
    capture_result_saver_.configure(config_.camera.image_capture_enabled,
                                    appRootFromConfigPath(config_path) / "camera_images");

    HubServerCallbacks callbacks;
#if TRACKCAMHUB_ENABLE_THRIFT
    callbacks.task_changed = [this](const auto& info) {
        capture_result_saver_.saveTaskInfo(info);
        workflow_.onTaskInfoChanged(info);
    };
#endif

    if (!hub_server_.start(config_.hub, std::move(callbacks)))
    {
        return false;
    }

    running_.store(true);
    camera_client_.startHeartbeat();

    if (!direct_trigger_server_.start(config_.direct_trigger, [this](const TrackSampleEvent& event) {
            workflow_.onTrackSampleReady(event);
        }))
    {
        stop();
        return false;
    }

    if (config_.track.enabled)
    {
        if (!track_listener_.start(config_.track, [this](const TrackSampleEvent& event) {
                workflow_.onTrackSampleReady(event);
            }))
        {
            
            stop();
            return false;
        }
    }
    else
    {
        Logger::warn("track serial listener disabled by config");
    }

    return true;
}

void TrackCamHubApp::stop()
{
    if (!running_.exchange(false))
    {
        return;
    }

    track_listener_.stop();
    direct_trigger_server_.stop();
    camera_client_.stopHeartbeat();
    hub_server_.stop();
    Logger::info("TrackCamHub stopped");
}

} // namespace trackcamhub
