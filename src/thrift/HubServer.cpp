#include "thrift/HubServer.h"

#include "app/Logger.h"

#if TRACKCAMHUB_ENABLE_THRIFT
#include <memory>
#include <thread>

#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>
#endif

namespace trackcamhub
{

#if TRACKCAMHUB_ENABLE_THRIFT
namespace
{

class SampleRegUCHandler final : public SampleReg::SampleRegUCIf
{
public:
    explicit SampleRegUCHandler(std::shared_ptr<HubServerCallbacks> callbacks)
        : callbacks_(std::move(callbacks))
    {
    }

    void HeartbeatToUC(int64_t timeStamp) override
    {
        (void)timeStamp;
    }

    int32_t DeviceInfoChanged(const SampleReg::DeviceInfo& info) override
    {
        (void)info;
        return 0;
    }

    int32_t TaskInfoChanged(const SampleReg::TaskInfo& info) override
    {
        Logger::info("TaskInfoChanged received, taskId=" + info.taskId);
        if (callbacks_->task_changed)
        {
            callbacks_->task_changed(info);
        }
        return 0;
    }

    int32_t OperInfoChanged(const SampleReg::GeneralOperInfo& info) override
    {
        (void)info;
        return 0;
    }

private:
    std::shared_ptr<HubServerCallbacks> callbacks_;
};

class SampleRegUCHandlerFactory final : public SampleReg::SampleRegUCIfFactory
{
public:
    explicit SampleRegUCHandlerFactory(std::shared_ptr<HubServerCallbacks> callbacks)
        : callbacks_(std::move(callbacks))
    {
    }

    SampleReg::SampleRegUCIf* getHandler(const apache::thrift::TConnectionInfo&) override
    {
        return new SampleRegUCHandler(callbacks_);
    }

    void releaseHandler(SampleReg::SampleRegUCIf* handler) override
    {
        delete handler;
    }

private:
    std::shared_ptr<HubServerCallbacks> callbacks_;
};

} // namespace
#endif

struct HubServer::Impl
{
#if TRACKCAMHUB_ENABLE_THRIFT
    std::shared_ptr<HubServerCallbacks> callbacks;
    std::unique_ptr<apache::thrift::server::TServer> server;
    std::thread worker;
#endif
};

HubServer::HubServer()
    : impl_(std::make_unique<Impl>())
{
}

HubServer::~HubServer()
{
    stop();
}

bool HubServer::start(const HubConfig& config, HubServerCallbacks callbacks)
{
    stop();

#if TRACKCAMHUB_ENABLE_THRIFT
    using apache::thrift::concurrency::ThreadFactory;
    using apache::thrift::concurrency::ThreadManager;
    using apache::thrift::protocol::TBinaryProtocolFactory;
    using apache::thrift::server::TThreadPoolServer;
    using apache::thrift::transport::TBufferedTransportFactory;
    using apache::thrift::transport::TServerSocket;

    try
    {
        impl_->callbacks = std::make_shared<HubServerCallbacks>(std::move(callbacks));

        auto handler_factory = std::make_shared<SampleRegUCHandlerFactory>(impl_->callbacks);
        auto processor_factory = std::make_shared<SampleReg::SampleRegUCProcessorFactory>(handler_factory);
        auto server_socket = std::make_shared<TServerSocket>(config.uc_server_port);
        auto transport_factory = std::make_shared<TBufferedTransportFactory>();
        auto protocol_factory = std::make_shared<TBinaryProtocolFactory>();

        auto thread_manager = ThreadManager::newSimpleThreadManager(8);
        thread_manager->threadFactory(std::make_shared<ThreadFactory>());
        thread_manager->start();

        impl_->server = std::make_unique<TThreadPoolServer>(
            processor_factory,
            server_socket,
            transport_factory,
            protocol_factory,
            thread_manager);

        impl_->worker = std::thread([this] {
            try
            {
                impl_->server->serve();
            }
            catch (const std::exception& ex)
            {
                Logger::error("UC thrift server stopped with error: " + std::string(ex.what()));
            }
        });

        Logger::info("UC thrift server started on port " + std::to_string(config.uc_server_port));
        return true;
    }
    catch (const std::exception& ex)
    {
        Logger::error("failed to start UC thrift server: " + std::string(ex.what()));
        return false;
    }
#else
    (void)config;
    (void)callbacks;
    Logger::warn("TrackCamHub was built without Thrift support; UC server disabled.");
    return true;
#endif
}

void HubServer::stop()
{
#if TRACKCAMHUB_ENABLE_THRIFT
    if (impl_->server)
    {
        impl_->server->stop();
    }
    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }
    impl_->server.reset();
    impl_->callbacks.reset();
#endif
}

} // namespace trackcamhub
