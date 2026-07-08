#pragma once

#include "config/AppConfig.h"

#include <functional>
#include <memory>
#include <string>

#if TRACKCAMHUB_ENABLE_THRIFT
#include "thrift/gen-cpp/SampleRegUC.h"
#endif

namespace trackcamhub
{

#if TRACKCAMHUB_ENABLE_THRIFT
struct ThriftServerCallbacks
{
    std::function<void(const SampleReg::TaskInfo&)> task_changed;   // 收到 task_changed 时调用
};
#else
struct HubServerCallbacks
{
};
#endif

class ThriftServer
{
public:
    ThriftServer();
    ~ThriftServer();

    ThriftServer(const ThriftServer&) = delete;
    ThriftServer& operator=(const ThriftServer&) = delete;

    bool start(const ThriftServerConfig& config, ThriftServerCallbacks callbacks);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace trackcamhub
