#pragma once

#include "config/AppConfig.h"
#include "serial/TrackSignalListener.h"
#include "thrift/CameraClient.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#if TRACKCAMHUB_ENABLE_THRIFT
#include "thrift/gen-cpp/SampleRegUC.h"
#endif

namespace trackcamhub
{

class CaptureResultSaver;

enum class WorkflowState
{
    Idle,
    Dispatching,
    WaitingResult,
    SavingResult,
    WaitingTrackRelease,
    ReleasingTrack,
    Error,
};

class CaptureWorkflow
{
public:
    using TrackReleaseSender = std::function<bool(const TrackSampleEvent&)>;

    CaptureWorkflow() = default;
    ~CaptureWorkflow();

    CaptureWorkflow(const CaptureWorkflow&) = delete;
    CaptureWorkflow& operator=(const CaptureWorkflow&) = delete;

    void configure(CameraConfig config,
                   std::string serial_port,
                   CameraClient* camera_client,
                   CaptureResultSaver* result_saver,
                   TrackReleaseSender track_release_sender);
    void stop();
    void onTrackSampleReady(const TrackSampleEvent& event);
    void onTrackReleaseReady();

#if TRACKCAMHUB_ENABLE_THRIFT
    bool onTaskInfoChanged(const SampleReg::TaskInfo& info);
#endif

    WorkflowState state() const;

private:
    struct PendingTask
    {
        TrackSampleEvent event;
        std::string task_id;
    };

    std::string makeTaskId();
    std::string logContext() const;
    bool waitForResult(const std::string& task_id);
    void run();
    void executeTask(const PendingTask& task);
    void resetTaskStateLocked(WorkflowState state);

    CameraConfig config_;
    std::string serial_port_;
    CameraClient* camera_client_ = nullptr;
    CaptureResultSaver* result_saver_ = nullptr;
    TrackReleaseSender track_release_sender_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    WorkflowState state_ = WorkflowState::Idle;
    bool stopping_ = true;
    std::thread worker_;
    std::optional<PendingTask> pending_task_;
    std::optional<TrackSampleEvent> current_event_;
    bool current_release_ready_ = false;
    std::string current_task_id_;
    bool current_finished_ = false;
    std::optional<int> current_ret_code_;
#if TRACKCAMHUB_ENABLE_THRIFT
    std::optional<SampleReg::TaskInfo> current_task_info_;
#endif
    std::uint64_t next_task_index_ = 0;
};

} // namespace trackcamhub
