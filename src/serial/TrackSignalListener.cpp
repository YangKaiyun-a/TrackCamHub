#include "serial/TrackSignalListener.h"

#include "app/Logger.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

namespace trackcamhub
{
namespace
{

constexpr std::uint8_t kFrameHead = 0x7E;
constexpr std::uint8_t kFrameTail = 0xE7;
constexpr std::uint8_t kEscape = 0x1B;
constexpr std::uint8_t kEscapeHead = 0xEA;
constexpr std::uint8_t kEscapeTail = 0xEB;
constexpr std::uint8_t kEscapeEscape = 0x00;

std::string toHex(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream stream;
    stream << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i > 0)
        {
            stream << ' ';
        }
        stream << "0x" << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return stream.str();
}

std::uint8_t checksum(const std::vector<std::uint8_t>& bytes, std::size_t count)
{
    std::uint8_t sum = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        sum = static_cast<std::uint8_t>(sum + bytes[i]);
    }
    return sum;
}

} // namespace

TrackSignalListener::~TrackSignalListener()
{
    stop();
}

bool TrackSignalListener::start(const TrackSerialConfig& config, Callback callback)
{
    stop();
    config_ = config;
    callback_ = std::move(callback);

    if (!serial_.open(config_.port, config_.baud_rate))
    {
        Logger::error("serial open failed on " + config_.port + ": " + serial_.lastError());
        return false;
    }

    stopping_.store(false);
    worker_ = std::thread([this] { run(); });
    Logger::info("track serial listener started on " + config_.port);
    return true;
}

void TrackSignalListener::stop()
{
    stopping_.store(true);
    serial_.close();
    if (worker_.joinable())
    {
        worker_.join();
    }
}

void TrackSignalListener::run()
{
    std::vector<std::uint8_t> frame;
    bool in_frame = false;
    bool escaping = false;

    while (!stopping_.load())
    {
        char ch = 0;
        if (!serial_.readByte(ch))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        const auto byte = static_cast<std::uint8_t>(ch);

        if (!in_frame)
        {
            if (byte == kFrameHead)
            {
                frame.clear();
                in_frame = true;
                escaping = false;
            }
            continue;
        }

        if (escaping)
        {
            if (byte == kEscapeHead)
            {
                frame.push_back(kFrameHead);
            }
            else if (byte == kEscapeTail)
            {
                frame.push_back(kFrameTail);
            }
            else if (byte == kEscapeEscape)
            {
                frame.push_back(kEscape);
            }
            else
            {
                Logger::warn("invalid serial escape sequence: 0x" + toHex({byte}));
                frame.clear();
                in_frame = false;
            }
            escaping = false;
            continue;
        }

        if (byte == kEscape)
        {
            escaping = true;
            continue;
        }

        if (byte == kFrameTail)
        {
            handleFrame(frame);
            frame.clear();
            in_frame = false;
            continue;
        }

        if (byte == kFrameHead)
        {
            Logger::warn("serial frame restarted before tail");
            frame.clear();
            escaping = false;
            continue;
        }

        frame.push_back(byte);
        if (frame.size() > 256)
        {
            Logger::warn("serial frame exceeded 256 bytes, dropping buffer");
            frame.clear();
            in_frame = false;
            escaping = false;
        }
    }
}

void TrackSignalListener::handleFrame(const std::vector<std::uint8_t>& frame)
{
    if (frame.size() < 5)
    {
        Logger::warn("serial frame too short: " + toHex(frame));
        return;
    }

    const auto expected_checksum = checksum(frame, frame.size() - 1);
    const auto actual_checksum = frame.back();
    if (actual_checksum != expected_checksum)
    {
        Logger::warn("serial checksum mismatch, frame=" + toHex(frame));
        return;
    }

    const std::uint16_t sequence = static_cast<std::uint16_t>((frame[0] << 8U) | frame[1]);
    const std::uint8_t gripper_id = frame[2];
    const std::uint8_t command = frame[3];

    Logger::debug("track serial frame: " + toHex(frame));
    if (command != static_cast<std::uint8_t>(config_.ready_command))
    {
        return;
    }

    TrackSampleEvent event;
    event.sample_id = "SEQ" + std::to_string(sequence) + "-G" + std::to_string(gripper_id);
    event.raw_message = toHex(frame);
    event.sequence = sequence;
    event.gripper_id = gripper_id;
    event.command = command;

    if (callback_)
    {
        callback_(event);
    }
}

} // namespace trackcamhub
