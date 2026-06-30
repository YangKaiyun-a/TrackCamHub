#include "app/TrackCamHubApp.h"

#include "app/Logger.h"
#include "config/AppConfig.h"

namespace trackcamhub
{

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
    image_capture_worker_.configure(config_.camera, &camera_client_);

    HubServerCallbacks callbacks;
#if TRACKCAMHUB_ENABLE_THRIFT
    callbacks.task_changed = [this](const auto& info) {
        workflow_.onTaskInfoChanged(info);
    };
    callbacks.oper_changed = [this](const auto& info) {
        image_capture_worker_.handleOperInfoChanged(info);
    };
#endif

    if (!hub_server_.start(config_.hub, std::move(callbacks)))
    {
        return false;
    }

    running_.store(true);
    camera_client_.startHeartbeat();
    if (!image_capture_worker_.start())
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
    image_capture_worker_.stop();
    camera_client_.stopHeartbeat();
    hub_server_.stop();
    Logger::info("TrackCamHub stopped");
}

} // namespace trackcamhub
