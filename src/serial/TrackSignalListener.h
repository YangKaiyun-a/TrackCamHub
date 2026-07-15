#pragma once

#include "config/AppConfig.h"
#include "serial/SerialPort.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace trackcamhub
{

struct TrackSampleEvent
{
    std::string sample_id;
    std::string raw_message;
    std::uint16_t protocol_sequence = 0;
    std::uint8_t protocol_gripper_id = 0;
    std::uint8_t command = 0;
    bool requires_track_release = false;
};

class TrackSignalListener
{
public:
    using Callback = std::function<void(const TrackSampleEvent&)>;
    using ReleaseReadyCallback = std::function<void()>;

    TrackSignalListener() = default;
    ~TrackSignalListener();

    bool start(const TrackSerialConfig& config,
               Callback callback,
               ReleaseReadyCallback release_ready_callback);
    void stop();
    bool sendTrackRelease(const TrackSampleEvent& event);

private:
    void run();
    void handleFrame(const std::vector<std::uint8_t>& frame);
    bool sendFrame(std::uint16_t sequence,
                   std::uint8_t gripper_id,
                   std::uint8_t command,
                   const char* label);
    std::string withPort(const std::string& message) const;

    TrackSerialConfig config_;
    Callback callback_;
    ReleaseReadyCallback release_ready_callback_;
    SerialPort serial_;
    std::mutex serial_write_mutex_;
    std::optional<TrackSampleEvent> pending_event_;
    std::atomic<bool> stopping_{true};
    std::thread worker_;
};

} // namespace trackcamhub
