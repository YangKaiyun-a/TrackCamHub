#include "workflow/CaptureWorkflow.h"

#include "app/Logger.h"
#include "workflow/CaptureResultSaver.h"

#include <chrono>
#include <ctime>
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
                                std::string serial_port,
                                CameraClient* camera_client,
                                CaptureResultSaver* result_saver,
                                TrackReleaseSender track_release_sender)
{
    stop();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = std::move(config);
        serial_port_ = std::move(serial_port);
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
        Logger::error(logContext() + "capture workflow has no camera client");
        return;
    }

    if (stopping_ || state_ != WorkflowState::Idle)
    {
        Logger::warn(logContext() + "drop sample-ready event while workflow is busy, raw=" + event.raw_message);
        return;
    }

    PendingTask task;
    task.event = event;
    task.task_id = makeTaskId();

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

void CaptureWorkflow::onTrackReleaseReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_event_ || !current_event_->requires_track_release)
    {
        Logger::warn(logContext() + "ignore track release-ready without an active track task");
        return;
    }

    current_release_ready_ = true;
    Logger::info(logContext() + "track release-ready matched current task, taskId=" + current_task_id_);
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

    Logger::info(logContext() + "TaskInfoChanged received, taskId=" + info.taskId +
                 ", state=" + std::to_string(static_cast<int>(info.state)) +
                 ", retCode=" + (info.__isset.retCode ? std::to_string(info.retCode) : "unset") +
                 ", result=" + (info.__isset.result ? "set" : "unset"));

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
    Logger::info(logContext() + "dispatch capture task: " + task.task_id);
    if (!camera_client_->distributeCaptureTask(task.task_id))
    {
        Logger::error(logContext() + "dispatch capture task failed, taskId=" + task.task_id +
                      ", error=" + camera_client_->lastError());
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
            resetTaskStateLocked(WorkflowState::Idle);
            Logger::warn(logContext() + "capture task timeout recovered; workflow is ready for the next task, taskId=" +
                         task.task_id);
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
            Logger::error(logContext() + "capture task finished without TaskInfo payload: " + task.task_id);
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

std::string CaptureWorkflow::makeTaskId()
{
    ++next_task_index_;

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) %
                              1000;

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream stream;
    stream << "TCH-" << config_.id << '-' << std::put_time(&tm, "%Y%m%dT%H%M%S")
           << std::setw(3) << std::setfill('0') << milliseconds.count() << "-N"
           << std::setw(6) << std::setfill('0') << next_task_index_;
    return stream.str();
}

std::string CaptureWorkflow::logContext() const
{
    return "[cameraId=" + config_.id + ", port=" + serial_port_ + "] ";
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
            Logger::warn(logContext() + "capture task finished with nonzero retCode, continue workflow, taskId=" + task_id +
                         ", retCode=" + std::to_string(*current_ret_code_));
        }
        return true;
    }

    Logger::error(logContext() + "capture task timeout: " + task_id);
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
