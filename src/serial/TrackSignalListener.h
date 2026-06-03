#pragma once

#include "config/AppConfig.h"
#include "serial/SerialPort.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace trackcamhub
{

struct TrackSampleEvent
{
    std::string sample_id;
    std::string raw_message;
    std::uint16_t sequence = 0;
    std::uint8_t gripper_id = 0;
    std::uint8_t command = 0;
};

class TrackSignalListener
{
public:
    using Callback = std::function<void(const TrackSampleEvent&)>;

    TrackSignalListener() = default;
    ~TrackSignalListener();

    bool start(const TrackSerialConfig& config, Callback callback);
    void stop();

private:
    void run();
    void handleFrame(const std::vector<std::uint8_t>& frame);

    TrackSerialConfig config_;
    Callback callback_;
    SerialPort serial_;
    std::atomic<bool> stopping_{true};
    std::thread worker_;
};

} // namespace trackcamhub
