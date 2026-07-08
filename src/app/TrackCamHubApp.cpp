#include "app/TrackCamHubApp.h"

#include "app/Logger.h"
#include "config/AppConfig.h"
#include "serial/TrackSignalListener.h"
#include "thrift/CameraClient.h"
#include "workflow/CaptureResultSaver.h"
#include "workflow/CaptureWorkflow.h"

#include <filesystem>
#include <memory>

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

struct TrackCamHubApp::CameraRuntime
{
    CameraConfig camera_config;
    TrackSerialConfig track_config;
    CameraClient camera_client;
    CaptureWorkflow workflow;
    CaptureResultSaver capture_result_saver;
    TrackSignalListener track_listener;
};

TrackCamHubApp::TrackCamHubApp() = default;

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

    cameras_.clear();
    const auto image_root = appRootFromConfigPath(config_path) / "camera_images";
    for (std::size_t i = 0; i < config_.cameras.size(); ++i)
    {
        auto runtime = std::make_unique<CameraRuntime>();
        runtime->camera_config = config_.cameras[i];
        runtime->track_config = config_.tracks[i];
        runtime->camera_client.configure(runtime->camera_config);
        runtime->workflow.configure(runtime->camera_config, &runtime->camera_client);
        runtime->capture_result_saver.configure(runtime->camera_config.image_capture_enabled,
                                                image_root / runtime->camera_config.id);
        cameras_.push_back(std::move(runtime));
    }

    ThriftServerCallbacks callbacks;
#if TRACKCAMHUB_ENABLE_THRIFT
    callbacks.task_changed = [this](const auto& info) {
        for (const auto& camera : cameras_)
        {
            if (camera->workflow.onTaskInfoChanged(info))
            {
                camera->capture_result_saver.saveTaskInfo(info);
                return;
            }
        }
        Logger::warn("ignore TaskInfoChanged for unknown taskId=" + info.taskId);
    };
#endif

    if (!thrift_server_.start(config_.hub, std::move(callbacks)))
    {
        return false;
    }

    running_.store(true);

    for (const auto& camera : cameras_)
    {
        camera->camera_client.startHeartbeat();
    }

    if (!direct_trigger_server_.start(config_.direct_trigger, [this](const TrackSampleEvent& event) {
            if (!cameras_.empty())
            {
                cameras_.front()->workflow.onTrackSampleReady(event);
            }
        }))
    {
        stop();
        return false;
    }

    bool any_serial_enabled = false;
    for (const auto& camera : cameras_)
    {
        if (!camera->track_config.enabled)
        {
            Logger::warn("track serial listener disabled by config, cameraId=" + camera->camera_config.id);
            continue;
        }

        any_serial_enabled = true;
        auto* runtime = camera.get();
        if (!runtime->track_listener.start(runtime->track_config, [runtime](const TrackSampleEvent& event) {
                runtime->workflow.onTrackSampleReady(event);
            }))
        {
            stop();
            return false;
        }
    }

    if (!any_serial_enabled)
    {
        Logger::warn("all track serial listeners disabled by config");
    }

    return true;
}

void TrackCamHubApp::stop()
{
    if (!running_.exchange(false))
    {
        return;
    }

    for (const auto& camera : cameras_)
    {
        camera->track_listener.stop();
    }
    direct_trigger_server_.stop();
    for (const auto& camera : cameras_)
    {
        camera->camera_client.stopHeartbeat();
    }
    thrift_server_.stop();
    cameras_.clear();
    Logger::info("TrackCamHub stopped");
}

} // namespace trackcamhub
