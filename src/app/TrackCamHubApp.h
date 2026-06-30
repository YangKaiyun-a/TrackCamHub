#pragma once

#include "config/AppConfig.h"
#include "serial/TrackSignalListener.h"
#include "thrift/CameraClient.h"
#include "thrift/HubServer.h"
#include "workflow/CameraImageCaptureWorker.h"
#include "workflow/CaptureWorkflow.h"

#include <atomic>

namespace trackcamhub
{

class TrackCamHubApp
{
public:
    TrackCamHubApp() = default;
    ~TrackCamHubApp();

    bool start(const std::string& config_path);
    void stop();

private:
    AppConfig config_;
    CameraClient camera_client_;
    HubServer hub_server_;
    CaptureWorkflow workflow_;
    CameraImageCaptureWorker image_capture_worker_;
    TrackSignalListener track_listener_;
    std::atomic<bool> running_{false};
};

} // namespace trackcamhub
