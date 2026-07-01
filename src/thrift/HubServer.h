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
struct HubServerCallbacks
{
    std::function<void(const SampleReg::TaskInfo&)> task_changed;
};
#else
struct HubServerCallbacks
{
};
#endif

class HubServer
{
public:
    HubServer();
    ~HubServer();

    HubServer(const HubServer&) = delete;
    HubServer& operator=(const HubServer&) = delete;

    bool start(const HubConfig& config, HubServerCallbacks callbacks);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace trackcamhub
