#include "serial/SerialPort.h"

#include <sstream>

namespace trackcamhub
{
namespace
{

#ifdef _WIN32
std::string windowsErrorMessage(DWORD error)
{
    std::ostringstream stream;
    stream << "Win32 error " << error;
    return stream.str();
}

std::string normalizePortName(const std::string& port_name)
{
    if (port_name.rfind("\\\\.\\", 0) == 0)
    {
        return port_name;
    }
    return "\\\\.\\" + port_name;
}
#endif

} // namespace

SerialPort::~SerialPort()
{
    close();
}

bool SerialPort::open(const std::string& port_name, int baud_rate)
{
    close();

#ifdef _WIN32
    const auto device_name = normalizePortName(port_name);
    handle_ = CreateFileA(device_name.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          nullptr,
                          OPEN_EXISTING,
                          0,
                          nullptr);

    if (handle_ == INVALID_HANDLE_VALUE)
    {
        last_error_ = "open serial port failed: " + windowsErrorMessage(GetLastError());
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb))
    {
        last_error_ = "GetCommState failed: " + windowsErrorMessage(GetLastError());
        close();
        return false;
    }

    dcb.BaudRate = static_cast<DWORD>(baud_rate);
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;

    if (!SetCommState(handle_, &dcb))
    {
        last_error_ = "SetCommState failed: " + windowsErrorMessage(GetLastError());
        close();
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(handle_, &timeouts);

    return true;
#else
    (void)port_name;
    (void)baud_rate;
    last_error_ = "SerialPort is currently implemented for Windows only.";
    return false;
#endif
}

void SerialPort::close()
{
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    open_ = false;
#endif
}

bool SerialPort::isOpen() const
{
#ifdef _WIN32
    return handle_ != INVALID_HANDLE_VALUE;
#else
    return open_;
#endif
}

bool SerialPort::readByte(char& value)
{
#ifdef _WIN32
    if (!isOpen())
    {
        last_error_ = "serial port is not open";
        return false;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(handle_, &value, 1, &bytes_read, nullptr))
    {
        last_error_ = "ReadFile failed: " + windowsErrorMessage(GetLastError());
        return false;
    }

    return bytes_read == 1;
#else
    (void)value;
    return false;
#endif
}

std::string SerialPort::lastError() const
{
    return last_error_;
}

} // namespace trackcamhub

