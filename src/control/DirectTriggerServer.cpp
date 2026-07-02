#include "control/DirectTriggerServer.h"

#include "app/Logger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

namespace trackcamhub
{
namespace
{

#ifdef _WIN32
constexpr SOCKET kInvalidSocket = INVALID_SOCKET;
#else
constexpr SOCKET kInvalidSocket = -1;
#endif

std::string trim(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string socketError()
{
#ifdef _WIN32
    return "WinSock error " + std::to_string(WSAGetLastError());
#else
    return "socket error";
#endif
}

} // namespace

DirectTriggerServer::~DirectTriggerServer()
{
    stop();
}

bool DirectTriggerServer::start(const DirectTriggerConfig& config, Callback callback)
{
    stop();

    config_ = config;
    callback_ = std::move(callback);

    if (!config_.enabled)
    {
        Logger::info("direct trigger server disabled by config");
        return true;
    }

#ifdef _WIN32
    WSADATA data{};
    const int startup_result = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup_result != 0)
    {
        Logger::error("WSAStartup failed: " + std::to_string(startup_result));
        return false;
    }
    winsock_started_ = true;
#endif

    listen_socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == kInvalidSocket)
    {
        Logger::error("direct trigger socket create failed: " + socketError());
#ifdef _WIN32
        WSACleanup();
        winsock_started_ = false;
#endif
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(config_.port));
    if (inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr) != 1)
    {
        Logger::error("invalid direct trigger host: " + config_.host);
        closeSocket(listen_socket_);
        listen_socket_ = kInvalidSocket;
        return false;
    }

    int reuse = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        Logger::error("direct trigger bind failed on " + config_.host + ":" +
                      std::to_string(config_.port) + ", " + socketError());
        closeSocket(listen_socket_);
        listen_socket_ = kInvalidSocket;
        return false;
    }

    if (listen(listen_socket_, SOMAXCONN) != 0)
    {
        Logger::error("direct trigger listen failed: " + socketError());
        closeSocket(listen_socket_);
        listen_socket_ = kInvalidSocket;
        return false;
    }

    stopping_.store(false);
    worker_ = std::thread([this] { run(); });
    Logger::info("direct trigger server started on " + config_.host + ":" + std::to_string(config_.port));
    return true;
}

void DirectTriggerServer::stop()
{
    stopping_.store(true);
    if (listen_socket_ != kInvalidSocket)
    {
        closeSocket(listen_socket_);
        listen_socket_ = kInvalidSocket;
    }

    if (worker_.joinable())
    {
        worker_.join();
    }

#ifdef _WIN32
    if (winsock_started_)
    {
        WSACleanup();
        winsock_started_ = false;
    }
#endif
}

void DirectTriggerServer::run()
{
    while (!stopping_.load())
    {
        const SOCKET client = accept(listen_socket_, nullptr, nullptr);
        if (client == kInvalidSocket)
        {
            if (!stopping_.load())
            {
                Logger::warn("direct trigger accept failed: " + socketError());
            }
            continue;
        }

        handleClient(client);
    }
}

void DirectTriggerServer::handleClient(SOCKET client)
{
    std::vector<char> buffer(512);
    const int bytes = recv(client, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
    if (bytes <= 0)
    {
        closeSocket(client);
        return;
    }

    std::string request(buffer.data(), static_cast<std::size_t>(bytes));
    request = trim(request);
    if (request.empty())
    {
        request = "CAPTURE";
    }

    const auto event = makeEvent(request);
    Logger::info("direct trigger request: " + event.sample_id + ", raw=" + event.raw_message);

    const char* response = "ACCEPTED\n";
    send(client, response, static_cast<int>(std::strlen(response)), 0);
    closeSocket(client);

    if (callback_)
    {
        callback_(event);
    }
}

TrackSampleEvent DirectTriggerServer::makeEvent(std::string request_text)
{
    const auto request_id = ++next_request_id_;

    TrackSampleEvent event;
    event.sample_id = "API" + std::to_string(request_id);
    event.raw_message = std::move(request_text);
    event.sequence = static_cast<std::uint16_t>(request_id & 0xFFFF);
    event.gripper_id = 0;
    event.command = 0;
    return event;
}

void DirectTriggerServer::closeSocket(SOCKET socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    (void)socket;
#endif
}

} // namespace trackcamhub
