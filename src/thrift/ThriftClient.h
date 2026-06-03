#pragma once

#if TRACKCAMHUB_ENABLE_THRIFT

#include <memory>
#include <string>

#include <thrift/Thrift.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>

namespace trackcamhub
{

template <typename Stub>
class ThriftClient
{
public:
    template <typename Func>
    bool call(const std::string& host, int port, Func&& func)
    {
        using apache::thrift::protocol::TBinaryProtocol;
        using apache::thrift::transport::TBufferedTransport;
        using apache::thrift::transport::TSocket;

        error_.clear();
        try
        {
            auto socket = std::make_shared<TSocket>(host, port);
            auto transport = std::make_shared<TBufferedTransport>(socket);
            auto protocol = std::make_shared<TBinaryProtocol>(transport);

            transport->open();
            Stub stub(protocol);
            func(stub);
            transport->close();
            return true;
        }
        catch (const apache::thrift::TException& ex)
        {
            error_ = ex.what();
        }
        catch (const std::exception& ex)
        {
            error_ = ex.what();
        }
        catch (...)
        {
            error_ = "unknown thrift client error";
        }
        return false;
    }

    const std::string& error() const
    {
        return error_;
    }

private:
    std::string error_;
};

} // namespace trackcamhub

#endif

