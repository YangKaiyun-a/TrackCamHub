#pragma once

#include "config/AppConfig.h"

#include <atomic>
#include <string>
#include <thread>

#if TRACKCAMHUB_ENABLE_THRIFT
#include "thrift/gen-cpp/SampleRegLC.h"
#endif

namespace trackcamhub
{

class CameraClient
{
public:
    CameraClient() = default;
    ~CameraClient();

    void configure(CameraConfig config);
    bool startHeartbeat();
    void stopHeartbeat();

    bool sendHeartbeatOnce();
    bool distributeCaptureTask(const std::string& task_id);
    std::string lastError() const;

private:
    void heartbeatLoop();

    CameraConfig config_;
    mutable std::string last_error_;
    std::atomic<bool> stopping_{true};
    std::thread heartbeat_thread_;
};

} // namespace trackcamhub
