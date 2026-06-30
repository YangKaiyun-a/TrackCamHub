#pragma once

#include <cstdint>
#include <string>

namespace trackcamhub
{

struct TrackSerialConfig
{
    bool enabled = false;
    std::string port = "COM3";
    int baud_rate = 115200;
    int ready_command = 0x00;
};

struct CameraConfig
{
    std::string id = "camera-1";
    std::string host = "127.0.0.1";
    int port = 9090;
    int heartbeat_interval_ms = 1000;
    int heartbeat_fail_max = 3;
    int capture_timeout_ms = 15000;
    int poll_interval_ms = 200;
    bool image_capture_enabled = false;
    int image_capture_interval_ms = 100;
    std::string image_capture_dir = "camera_images";
};

struct HubConfig
{
    int uc_server_port = 9091;
};

struct AppConfig
{
    HubConfig hub;
    TrackSerialConfig track;
    CameraConfig camera;
};

class AppConfigLoader
{
public:
    static AppConfig load(const std::string& path);
};

} // namespace trackcamhub
