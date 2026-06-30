#include "thrift/CameraClient.h"

#include "app/Logger.h"

#if TRACKCAMHUB_ENABLE_THRIFT
#include "thrift/ThriftClient.h"
#endif

#include <chrono>
#include <utility>

namespace trackcamhub
{

CameraClient::~CameraClient()
{
    stopHeartbeat();
}

void CameraClient::configure(CameraConfig config)
{
    config_ = std::move(config);
    connected_.store(false);
}

bool CameraClient::startHeartbeat()
{
    stopHeartbeat();
    connected_.store(false);
    stopping_.store(false);
    heartbeat_thread_ = std::thread([this] { heartbeatLoop(); });
    return true;
}

void CameraClient::stopHeartbeat()
{
    stopping_.store(true);
    connected_.store(false);
    if (heartbeat_thread_.joinable())
    {
        heartbeat_thread_.join();
    }
}

bool CameraClient::isConnected() const
{
    return connected_.load();
}

bool CameraClient::sendHeartbeatOnce()
{
#if TRACKCAMHUB_ENABLE_THRIFT
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    ThriftClient<SampleReg::SampleRegLCClient> client;
    const bool ok = client.call(config_.host, config_.port, [&](auto& stub) {
        stub.HeartbeatToLC(timestamp);
    });

    if (!ok)
    {
        setLastError(client.error());
    }
    return ok;
#else
    setLastError("TrackCamHub was built without Thrift support.");
    return false;
#endif
}

bool CameraClient::distributeCaptureTask(const std::string& task_id)
{
#if TRACKCAMHUB_ENABLE_THRIFT
    SampleReg::TaskInfo task;
    task.__set_taskId(task_id);
    task.__set_mode(2);
    task.__set_taskType({
        SampleReg::TaskType::TASK_GET_BARCODE,
        SampleReg::TaskType::TASK_GET_BEST_BARCODE_IMAGE,
    });
    task.__set_state(SampleReg::TaskState::Issued);

    ThriftClient<SampleReg::SampleRegLCClient> client;
    int32_t ret = -1;
    const bool ok = client.call(config_.host, config_.port, [&](auto& stub) {
        ret = stub.DistributeTask(task);
    });

    if (!ok)
    {
        setLastError(client.error());
        return false;
    }

    if (ret != 0)
    {
        setLastError("DistributeTask returned " + std::to_string(ret));
        return false;
    }

    return true;
#else
    setLastError("TrackCamHub was built without Thrift support.");
    return false;
#endif
}

bool CameraClient::requestCameraImage()
{
#if TRACKCAMHUB_ENABLE_THRIFT
    SampleReg::GeneralOperInfo info;
    info.__set_cmd(SampleReg::GeneralOperCmd::GetCameraImage);

    ThriftClient<SampleReg::SampleRegLCClient> client;
    int32_t ret = -1;
    const bool ok = client.call(config_.host, config_.port, [&](auto& stub) {
        ret = stub.DistributeOper(info);
    });

    if (!ok)
    {
        setLastError(client.error());
        return false;
    }

    if (ret != 0)
    {
        setLastError("DistributeOper(GetCameraImage) returned " + std::to_string(ret));
        return false;
    }

    return true;
#else
    setLastError("TrackCamHub was built without Thrift support.");
    return false;
#endif
}

std::string CameraClient::lastError() const
{
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void CameraClient::setLastError(std::string error)
{
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = std::move(error);
}

void CameraClient::heartbeatLoop()
{
    int fail_count = 0;
    bool ever_connected = false;

    while (!stopping_.load())
    {
        if (sendHeartbeatOnce())
        {
            connected_.store(true);
            if (!ever_connected)
            {
                Logger::info("camera heartbeat connected: " + config_.host + ":" + std::to_string(config_.port));
                ever_connected = true;
            }
            fail_count = 0;
        }
        else
        {
            ++fail_count;
            Logger::warn("camera heartbeat failed: " + lastError());
            if (fail_count >= config_.heartbeat_fail_max)
            {
                connected_.store(false);
                Logger::error("camera heartbeat lost: " + config_.id);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeat_interval_ms));
    }
}

} // namespace trackcamhub
