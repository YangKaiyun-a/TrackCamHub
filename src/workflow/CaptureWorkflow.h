#pragma once

#include "config/AppConfig.h"
#include "serial/TrackSignalListener.h"
#include "thrift/CameraClient.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#if TRACKCAMHUB_ENABLE_THRIFT
#include "thrift/gen-cpp/SampleRegUC.h"
#endif

namespace trackcamhub
{

enum class WorkflowState
{
    Idle,
    Dispatching,
    WaitingResult,
    Error,
};

class CaptureWorkflow
{
public:
    void configure(CameraConfig config, CameraClient* camera_client);
    void onTrackSampleReady(const TrackSampleEvent& event);

#if TRACKCAMHUB_ENABLE_THRIFT
    void onTaskInfoChanged(const SampleReg::TaskInfo& info);
#endif

    WorkflowState state() const;

private:
    std::string makeTaskId(const TrackSampleEvent& event);
    bool waitForResult(const std::string& task_id);

    CameraConfig config_;
    CameraClient* camera_client_ = nullptr;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    WorkflowState state_ = WorkflowState::Idle;
    std::string current_task_id_;
    bool current_finished_ = false;
    std::optional<int> current_ret_code_;
    std::uint64_t next_task_index_ = 0;
};

} // namespace trackcamhub
