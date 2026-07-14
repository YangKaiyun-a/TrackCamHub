#include "workflow/CaptureWorkflow.h"

#include "app/Logger.h"
#include "workflow/CaptureResultSaver.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>

namespace trackcamhub
{

CaptureWorkflow::~CaptureWorkflow()
{
    stop();
}

void CaptureWorkflow::configure(CameraConfig config,
                                CameraClient* camera_client,
                                CaptureResultSaver* result_saver,
                                TrackReleaseSender track_release_sender)
{
    stop();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = std::move(config);
        camera_client_ = camera_client;
        result_saver_ = result_saver;
        track_release_sender_ = std::move(track_release_sender);
        stopping_ = false;
    }

    worker_ = std::thread([this] { run(); });
}

void CaptureWorkflow::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        pending_task_.reset();
        cv_.notify_all();
    }

    if (worker_.joinable())
    {
        worker_.join();
    }
}

// Starts a task without blocking the serial listener thread.
void CaptureWorkflow::onTrackSampleReady(const TrackSampleEvent& event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!camera_client_)
    {
        Logger::error("capture workflow has no camera client");
        return;
    }

    if (stopping_ || state_ != WorkflowState::Idle)
    {
        Logger::warn("drop sample-ready event while workflow is busy, raw=" + event.raw_message);
        return;
    }

    PendingTask task;
    task.event = event;
    task.task_id = makeTaskId(event);

    state_ = WorkflowState::Dispatching;
    current_task_id_ = task.task_id;
    current_event_ = event;
    current_finished_ = false;
    current_ret_code_.reset();
    current_release_ready_ = false;
#if TRACKCAMHUB_ENABLE_THRIFT
    current_task_info_.reset();
#endif
    pending_task_ = std::move(task);
    cv_.notify_all();
}

void CaptureWorkflow::onTrackReleaseReady(std::uint16_t sequence, std::uint8_t gripper_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_event_ || !current_event_->requires_track_release ||
        current_event_->sequence != sequence || current_event_->gripper_id != gripper_id)
    {
        Logger::warn("ignore track release-ready for unrelated task, sequence=" + std::to_string(sequence) +
                     ", gripper=" + std::to_string(gripper_id));
        return;
    }

    current_release_ready_ = true;
    Logger::info("track release-ready matched current task, sequence=" + std::to_string(sequence) +
                 ", gripper=" + std::to_string(gripper_id));
    cv_.notify_all();
}

#if TRACKCAMHUB_ENABLE_THRIFT
bool CaptureWorkflow::onTaskInfoChanged(const SampleReg::TaskInfo& info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (info.taskId != current_task_id_)
    {
        return false;
    }

    if (info.__isset.retCode)
    {
        current_ret_code_ = info.retCode;
    }

    if (info.state == SampleReg::TaskState::Finished)
    {
        current_task_info_ = info;
        current_finished_ = true;
        cv_.notify_all();
    }

    return true;
}
#endif

WorkflowState CaptureWorkflow::state() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void CaptureWorkflow::run()
{
    while (true)
    {
        PendingTask task;
        {
            std::unique_lock<std::mutex> lock(mutex_);

            // 等待轨道信号
            cv_.wait(lock, [this] { return stopping_ || pending_task_.has_value(); });
            if (stopping_)
            {
                return;
            }

            task = std::move(*pending_task_);
            pending_task_.reset();
        }

        executeTask(task);
    }
}

void CaptureWorkflow::executeTask(const PendingTask& task)
{
    Logger::info("dispatch capture task: " + task.task_id);
    if (!camera_client_->distributeCaptureTask(task.task_id))
    {
        Logger::error("dispatch capture task failed: " + camera_client_->lastError());
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stopping_)
        {
            resetTaskStateLocked(WorkflowState::Error);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
        {
            return;
        }
        state_ = WorkflowState::WaitingResult;
    }

    // 等待相机拍照完成
    if (!waitForResult(task.task_id))
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stopping_)
        {
            resetTaskStateLocked(WorkflowState::Error);
        }
        return;
    }

#if TRACKCAMHUB_ENABLE_THRIFT
    SampleReg::TaskInfo task_info;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
        {
            return;
        }
        if (!current_task_info_)
        {
            Logger::error("capture task finished without TaskInfo payload: " + task.task_id);
            resetTaskStateLocked(WorkflowState::Error);
            return;
        }

        // 拍照完成后更新状态
        task_info = *current_task_info_;
        state_ = WorkflowState::SavingResult;
    }

    // 存图
    if (result_saver_)
    {
        result_saver_->saveTaskInfo(task_info);
    }
#endif

    if (!task.event.requires_track_release)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stopping_)
        {
            resetTaskStateLocked(WorkflowState::Idle);
        }
        return;
    }

    // 等待轨道的释放信号
    {
        std::unique_lock<std::mutex> lock(mutex_);
        state_ = WorkflowState::WaitingTrackRelease;
        cv_.wait(lock, [this] { return stopping_ || current_release_ready_; });
        if (stopping_)
        {
            return;
        }
        state_ = WorkflowState::ReleasingTrack;
    }

    // 释放当前试管
    const bool released = track_release_sender_ && track_release_sender_(task.event);
    std::lock_guard<std::mutex> lock(mutex_);
    resetTaskStateLocked(released ? WorkflowState::Idle : WorkflowState::Error);
}

std::string CaptureWorkflow::makeTaskId(const TrackSampleEvent& event)
{
    ++next_task_index_;

    std::ostringstream stream;
    stream << "TCH-" << config_.id << "-" << event.sample_id << "-N"
           << std::setw(6) << std::setfill('0') << next_task_index_;
    return stream.str();
}

bool CaptureWorkflow::waitForResult(const std::string& task_id)
{
    const auto timeout = std::chrono::milliseconds(config_.capture_timeout_ms);
    std::unique_lock<std::mutex> lock(mutex_);
    const bool callback_done = cv_.wait_for(lock, timeout, [this] {
        return stopping_ || current_finished_;
    });

    if (stopping_)
    {
        return false;
    }

    if (callback_done)
    {
        if (current_ret_code_ && *current_ret_code_ != 0)
        {
            Logger::warn("capture task finished with nonzero retCode, continue workflow, taskId=" + task_id +
                         ", retCode=" + std::to_string(*current_ret_code_));
        }
        return true;
    }

    Logger::error("capture task timeout: " + task_id);
    return false;
}

void CaptureWorkflow::resetTaskStateLocked(WorkflowState state)
{
    state_ = state;
    current_task_id_.clear();
    current_event_.reset();
    current_finished_ = false;
    current_ret_code_.reset();
    current_release_ready_ = false;
#if TRACKCAMHUB_ENABLE_THRIFT
    current_task_info_.reset();
#endif
}

} // namespace trackcamhub
