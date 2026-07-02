#pragma once

#include "config/AppConfig.h"
#include "serial/TrackSignalListener.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
using SOCKET = int;
#endif

namespace trackcamhub
{

class DirectTriggerServer
{
public:
    using Callback = std::function<void(const TrackSampleEvent&)>;

    DirectTriggerServer() = default;
    ~DirectTriggerServer();

    DirectTriggerServer(const DirectTriggerServer&) = delete;
    DirectTriggerServer& operator=(const DirectTriggerServer&) = delete;

    bool start(const DirectTriggerConfig& config, Callback callback);
    void stop();

private:
    void run();
    void handleClient(SOCKET client);
    TrackSampleEvent makeEvent(std::string request_text);
    void closeSocket(SOCKET socket);

    DirectTriggerConfig config_;
    Callback callback_;
    std::atomic<bool> stopping_{true};
    std::thread worker_;
    SOCKET listen_socket_ = static_cast<SOCKET>(-1);
    std::atomic<unsigned long long> next_request_id_{0};
    bool winsock_started_ = false;
};

} // namespace trackcamhub
