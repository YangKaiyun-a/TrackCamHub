#include "config/AppConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

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

std::string requireString(const std::unordered_map<std::string, std::string>& values, const std::string& key)
{
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty())
    {
        throw std::runtime_error("missing required config key: " + key);
    }
    return it->second;
}

std::vector<std::string> splitCsv(const std::string& value)
{
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= value.size())
    {
        const auto end = value.find(',', start);
        auto item = trim(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!item.empty())
        {
            items.push_back(std::move(item));
        }
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return items;
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
    config.direct_trigger.enabled = toBool(toString(values,
                                                    "direct_trigger.enabled",
                                                    config.direct_trigger.enabled ? "true" : "false"));
    config.direct_trigger.host = toString(values, "direct_trigger.host", config.direct_trigger.host);
    config.direct_trigger.port = toInt(values, "direct_trigger.port", config.direct_trigger.port);

    const auto camera_ids = splitCsv(toString(values, "camera.ids", ""));
    if (camera_ids.empty())
    {
        throw std::runtime_error("missing required config key: camera.ids");
    }

    for (const auto& camera_id : camera_ids)
    {
        CameraConfig camera;
        camera.id = camera_id;

        const auto camera_prefix = "camera." + camera_id + ".";
        camera.host = requireString(values, camera_prefix + "host");
        camera.port = toInt(values, camera_prefix + "port", camera.port);
        camera.heartbeat_interval_ms = toInt(values,
                                             camera_prefix + "heartbeat_interval_ms",
                                             camera.heartbeat_interval_ms);
        camera.heartbeat_fail_max = toInt(values,
                                          camera_prefix + "heartbeat_fail_max",
                                          camera.heartbeat_fail_max);
        camera.capture_timeout_ms = toInt(values,
                                          camera_prefix + "capture_timeout_ms",
                                          camera.capture_timeout_ms);
        camera.poll_interval_ms = toInt(values,
                                        camera_prefix + "poll_interval_ms",
                                        camera.poll_interval_ms);
        camera.save_image_out_enabled = toBool(toString(values,
                                                        camera_prefix + "save_image_out_enabled",
                                                        camera.save_image_out_enabled ? "true" : "false"));

        TrackSerialConfig track;
        const auto track_prefix = "track." + camera_id + ".";
        track.enabled = toBool(toString(values,
                                        track_prefix + "serial_enabled",
                                        track.enabled ? "true" : "false"));
        track.port = requireString(values, track_prefix + "port");
        track.baud_rate = toInt(values, track_prefix + "baud_rate", track.baud_rate);

        config.cameras.push_back(std::move(camera));
        config.tracks.push_back(std::move(track));
    }

    return config;
}

} // namespace trackcamhub
