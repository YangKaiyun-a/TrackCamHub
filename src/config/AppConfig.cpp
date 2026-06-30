#include "config/AppConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace trackcamhub
{
namespace
{

std::string trim(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool toBool(const std::string& value)
{
    auto normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on";
}

int toInt(const std::unordered_map<std::string, std::string>& values, const std::string& key, int fallback)
{
    const auto it = values.find(key);
    if (it == values.end())
    {
        return fallback;
    }
    return std::stoi(it->second);
}

int toIntAutoBase(const std::unordered_map<std::string, std::string>& values, const std::string& key, int fallback)
{
    const auto it = values.find(key);
    if (it == values.end())
    {
        return fallback;
    }
    return std::stoi(it->second, nullptr, 0);
}

std::string toString(const std::unordered_map<std::string, std::string>& values,
                     const std::string& key,
                     const std::string& fallback)
{
    const auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

} // namespace

AppConfig AppConfigLoader::load(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("cannot open config file: " + path);
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line))
    {
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';')
        {
            continue;
        }

        const auto pos = line.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }

        values[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }

    AppConfig config;
    config.hub.uc_server_port = toInt(values, "hub.uc_server_port", config.hub.uc_server_port);

    config.track.enabled = toBool(toString(values, "track.serial_enabled", config.track.enabled ? "true" : "false"));
    config.track.port = toString(values, "track.port", config.track.port);
    config.track.baud_rate = toInt(values, "track.baud_rate", config.track.baud_rate);
    config.track.ready_command = toIntAutoBase(values, "track.ready_command", config.track.ready_command);

    config.camera.id = toString(values, "camera.id", config.camera.id);
    config.camera.host = toString(values, "camera.host", config.camera.host);
    config.camera.port = toInt(values, "camera.port", config.camera.port);
    config.camera.heartbeat_interval_ms = toInt(values, "camera.heartbeat_interval_ms", config.camera.heartbeat_interval_ms);
    config.camera.heartbeat_fail_max = toInt(values, "camera.heartbeat_fail_max", config.camera.heartbeat_fail_max);
    config.camera.capture_timeout_ms = toInt(values, "camera.capture_timeout_ms", config.camera.capture_timeout_ms);
    config.camera.poll_interval_ms = toInt(values, "camera.poll_interval_ms", config.camera.poll_interval_ms);
    config.camera.image_capture_enabled = toBool(toString(values,
                                                          "camera.image_capture_enabled",
                                                          config.camera.image_capture_enabled ? "true" : "false"));
    config.camera.image_capture_interval_ms = toInt(values,
                                                    "camera.image_capture_interval_ms",
                                                    config.camera.image_capture_interval_ms);
    config.camera.image_capture_dir = toString(values,
                                               "camera.image_capture_dir",
                                               config.camera.image_capture_dir);

    return config;
}

} // namespace trackcamhub
