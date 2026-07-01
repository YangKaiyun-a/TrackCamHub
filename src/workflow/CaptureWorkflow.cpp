#include "workflow/CaptureWorkflow.h"

#include "app/Logger.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace trackcamhub
{

void CaptureWorkflow::configure(CameraConfig config, CameraClient* camera_client)
{
    config_ = std::move(config);
    camera_client_ = camera_client;
}

void CaptureWorkflow::onTrackSampleReady(const TrackSampleEvent& event)
{
    if (!camera_client_)
    {
        Logger::error("capture workflow has no camera client");
        return;
    }

    std::string task_id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != WorkflowState::Idle)
        {
            Logger::warn("drop sample-ready event while workflow is busy, raw=" + event.raw_message);
            return;
        }
        task_id = makeTaskId(event);
        state_ = WorkflowState::Dispatching;
        current_task_id_ = task_id;
        current_finished_ = false;
        current_ret_code_.reset();
    }

    Logger::info("dispatch capture task: " + task_id);
    if (!camera_client_->distributeCaptureTask(task_id))
    {
        Logger::error("dispatch capture task failed: " + camera_client_->lastError());
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = WorkflowState::Error;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = WorkflowState::WaitingResult;
    }

    const bool ok = waitForResult(task_id);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ok ? WorkflowState::Idle : WorkflowState::Error;
        current_task_id_.clear();
        current_finished_ = false;
        current_ret_code_.reset();
    }
}

#if TRACKCAMHUB_ENABLE_THRIFT
void CaptureWorkflow::onTaskInfoChanged(const SampleReg::TaskInfo& info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (info.taskId != current_task_id_)
    {
        Logger::warn("ignore TaskInfoChanged for unknown taskId=" + info.taskId);
        return;
    }

    if (info.__isset.retCode)
    {
        current_ret_code_ = info.retCode;
    }

    if (info.state == SampleReg::TaskState::Finished)
    {
        current_finished_ = true;
        cv_.notify_all();
    }
}
#endif

WorkflowState CaptureWorkflow::state() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::string CaptureWorkflow::makeTaskId(const TrackSampleEvent& event)
{
    ++next_task_index_;

    std::ostringstream stream;
    stream << "TCH-" << event.sample_id << "-N"
           << std::setw(6) << std::setfill('0') << next_task_index_;
    return stream.str();
}

bool CaptureWorkflow::waitForResult(const std::string& task_id)
{
    const auto timeout = std::chrono::milliseconds(config_.capture_timeout_ms);
    std::unique_lock<std::mutex> lock(mutex_);
    const bool callback_done = cv_.wait_for(lock, timeout, [&] {
        return current_finished_;
    });

    if (callback_done)
    {
        const bool ok = current_ret_code_.value_or(-1) == 0;
        return ok;
    }

    Logger::error("capture task timeout: " + task_id);
    return false;
}

} // namespace trackcamhub
