#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif

namespace trackcamhub
{

class SerialPort
{
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const std::string& port_name, int baud_rate);
    void close();
    bool isOpen() const;
    bool readByte(char& value);
    bool writeBytes(const std::vector<std::uint8_t>& bytes);
    std::string lastError() const;

private:
    std::string last_error_;

#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    bool open_ = false;
#endif
};

} // namespace trackcamhub
