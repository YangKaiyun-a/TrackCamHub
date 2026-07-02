#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

int parseInt(const std::string& value)
{
    return std::stoi(value, nullptr, 0);
}

void printUsage()
{
    std::cout
        << "Usage:\n"
        << "  TriggerCapture.exe [--host 127.0.0.1] [--port 7090] [--message CAPTURE]\n";
}

} // namespace

int main(int argc, char* argv[])
{
    std::string host = "127.0.0.1";
    int port = 7090;
    std::string message = "CAPTURE";

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const auto requireValue = [&](const char* name) -> std::string {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return 0;
        }
        if (arg == "--host") host = requireValue("--host");
        else if (arg == "--port") port = parseInt(requireValue("--port"));
        else if (arg == "--message") message = requireValue("--message");
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage();
            return 2;
        }
    }

    WSADATA data{};
    const int startup_result = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup_result != 0)
    {
        std::cerr << "WSAStartup failed: " << startup_result << "\n";
        return 1;
    }

    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET)
    {
        std::cerr << "socket failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1)
    {
        std::cerr << "invalid host: " << host << "\n";
        closesocket(socket_handle);
        WSACleanup();
        return 1;
    }

    if (connect(socket_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        std::cerr << "connect failed: " << WSAGetLastError() << "\n";
        closesocket(socket_handle);
        WSACleanup();
        return 1;
    }

    message += "\n";
    const int sent = send(socket_handle, message.c_str(), static_cast<int>(message.size()), 0);
    if (sent <= 0)
    {
        std::cerr << "send failed: " << WSAGetLastError() << "\n";
        closesocket(socket_handle);
        WSACleanup();
        return 1;
    }

    char response[128]{};
    const int received = recv(socket_handle, response, sizeof(response) - 1, 0);
    if (received > 0)
    {
        std::cout << response;
    }

    closesocket(socket_handle);
    WSACleanup();
    return 0;
}
