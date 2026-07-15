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
constexpr std::uint8_t kCameraPositionCommand = 0x00;
constexpr std::uint8_t kCameraAckCommand = 0x00;
constexpr std::uint8_t kRotationSuccessCommand = 0x2C;
constexpr std::uint8_t kRotationFailureCommand = 0x29;
constexpr std::uint8_t kTrackReadyToReleaseCommand = 0x3C;
constexpr std::uint8_t kTrackReleaseCommand = 0x4C;

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

void appendEscaped(std::vector<std::uint8_t>& output, std::uint8_t byte)
{
    if (byte == kFrameHead)
    {
        output.push_back(kEscape);
        output.push_back(kEscapeHead);
    }
    else if (byte == kFrameTail)
    {
        output.push_back(kEscape);
        output.push_back(kEscapeTail);
    }
    else if (byte == kEscape)
    {
        output.push_back(kEscape);
        output.push_back(kEscapeEscape);
    }
    else
    {
        output.push_back(byte);
    }
}

std::vector<std::uint8_t> buildFrame(std::uint16_t sequence,
                                     std::uint8_t gripper_id,
                                     std::uint8_t command)
{
    std::vector<std::uint8_t> body;
    body.push_back(static_cast<std::uint8_t>((sequence >> 8U) & 0xFFU));
    body.push_back(static_cast<std::uint8_t>(sequence & 0xFFU));
    body.push_back(gripper_id);
    body.push_back(command);
    body.push_back(checksum(body, body.size()));

    std::vector<std::uint8_t> frame;
    frame.push_back(kFrameHead);
    for (const auto byte : body)
    {
        appendEscaped(frame, byte);
    }
    frame.push_back(kFrameTail);
    return frame;
}

} // namespace

TrackSignalListener::~TrackSignalListener()
{
    stop();
}

bool TrackSignalListener::start(const TrackSerialConfig& config,
                                Callback callback,
                                ReleaseReadyCallback release_ready_callback)
{
    stop();
    config_ = config;
    callback_ = std::move(callback);
    release_ready_callback_ = std::move(release_ready_callback);

    if (!serial_.open(config_.port, config_.baud_rate))
    {
        Logger::error(withPort("serial open failed: " + serial_.lastError()));
        return false;
    }

    stopping_.store(false);
    worker_ = std::thread([this] { run(); });
    Logger::info(withPort("track serial listener started"));
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

bool TrackSignalListener::sendTrackRelease(const TrackSampleEvent& event)
{
    return sendFrame(event.protocol_sequence,
                     event.protocol_gripper_id,
                     kTrackReleaseCommand,
                     "track release command");
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
                Logger::warn(withPort("invalid serial escape sequence: " + toHex({byte})));
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
            Logger::warn(withPort("serial frame restarted before tail"));
            frame.clear();
            escaping = false;
            continue;
        }

        frame.push_back(byte);
        if (frame.size() > 256)
        {
            Logger::warn(withPort("serial frame exceeded 256 bytes, dropping buffer"));
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
        Logger::warn(withPort("serial frame too short: " + toHex(frame)));
        return;
    }

    const auto expected_checksum = checksum(frame, frame.size() - 1);
    const auto actual_checksum = frame.back();
    if (actual_checksum != expected_checksum)
    {
        Logger::warn(withPort("serial checksum mismatch, frame=" + toHex(frame)));
        return;
    }

    const std::uint16_t sequence = static_cast<std::uint16_t>((frame[0] << 8U) | frame[1]);
    const std::uint8_t gripper_id = frame[2];
    const std::uint8_t command = frame[3];

    Logger::debug(withPort("track serial received: " + toHex(frame)));
    const bool is_short_frame = frame.size() == 5;
    const bool is_camera_position_frame = frame.size() == 8 && command == kCameraPositionCommand;
    if (!is_short_frame && !is_camera_position_frame)
    {
        Logger::warn(withPort("unexpected track frame size or command, bytes=" +
                              std::to_string(frame.size()) + ", command=" + toHex({command})));
        return;
    }

    if (command == kTrackReadyToReleaseCommand)
    {
        Logger::info(withPort("track ready to release"));
        if (release_ready_callback_)
        {
            release_ready_callback_();
        }
        return;
    }

    if (command == kRotationSuccessCommand)
    {
        Logger::info(withPort("track rotation start"));
        if (!pending_event_)
        {
            Logger::warn(withPort("ignore rotation success without pending capture event"));
            return;
        }

        auto event = *pending_event_;
        pending_event_.reset();
        if (callback_)
        {
            callback_(event);
        }
        return;
    }

    if (command == kRotationFailureCommand)
    {
        Logger::warn(withPort("track rotation failed"));
        pending_event_.reset();
        return;
    }

    if (!is_camera_position_frame)
    {
        return;
    }

    if (!sendFrame(sequence, gripper_id, kCameraAckCommand, "track serial reply"))
    {
        return;
    }

    TrackSampleEvent event;
    event.sample_id = "TRACK";
    event.raw_message = toHex(frame);
    event.protocol_sequence = sequence;
    event.protocol_gripper_id = gripper_id;
    event.command = command;
    event.requires_track_release = true;

    if (pending_event_)
    {
        Logger::warn(withPort("replace pending capture event before rotation result"));
    }
    pending_event_ = event;
}

bool TrackSignalListener::sendFrame(std::uint16_t sequence,
                                    std::uint8_t gripper_id,
                                    std::uint8_t command,
                                    const char* label)
{
    const auto frame = buildFrame(sequence, gripper_id, command);
    std::lock_guard<std::mutex> lock(serial_write_mutex_);
    if (!serial_.writeBytes(frame))
    {
        Logger::error(withPort(std::string(label) + " failed: " + serial_.lastError()));
        return false;
    }

    Logger::info(withPort(std::string(label) + " sent: " + toHex(frame)));
    return true;
}

std::string TrackSignalListener::withPort(const std::string& message) const
{
    return "[port=" + config_.port + "] " + message;
}

} // namespace trackcamhub
