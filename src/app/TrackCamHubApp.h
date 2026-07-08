#pragma once

#include "config/AppConfig.h"
#include "control/DirectTriggerServer.h"
#include "thrift/ThriftServer.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace trackcamhub
{

class TrackCamHubApp
{
public:
    TrackCamHubApp();
    ~TrackCamHubApp();

    bool start(const std::string& config_path);
    void stop();

private:
    struct CameraRuntime;

    AppConfig config_;
    ThriftServer thrift_server_;
    DirectTriggerServer direct_trigger_server_;
    std::vector<std::unique_ptr<CameraRuntime>> cameras_;
    std::atomic<bool> running_{false};
};

} // namespace trackcamhub
