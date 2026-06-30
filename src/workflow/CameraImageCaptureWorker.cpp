#include "workflow/CameraImageCaptureWorker.h"

#include "app/Logger.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace trackcamhub
{

CameraImageCaptureWorker::~CameraImageCaptureWorker()
{
    stop();
}

void CameraImageCaptureWorker::configure(CameraConfig config, CameraClient* camera_client)
{
    config_ = std::move(config);
    camera_client_ = camera_client;
}

bool CameraImageCaptureWorker::start()
{
    stop();

    if (!config_.image_capture_enabled)
    {
        return true;
    }

    if (camera_client_ == nullptr)
    {
        Logger::error("camera image capture worker has no camera client");
        return false;
    }

    std::filesystem::create_directories(config_.image_capture_dir);
    stopping_.store(false);
    worker_ = std::thread([this] { run(); });
    Logger::info("camera image capture enabled, output=" + config_.image_capture_dir);
    return true;
}

void CameraImageCaptureWorker::stop()
{
    stopping_.store(true);
    if (worker_.joinable())
    {
        worker_.join();
    }
}

#if TRACKCAMHUB_ENABLE_THRIFT
void CameraImageCaptureWorker::handleOperInfoChanged(const SampleReg::GeneralOperInfo& info)
{
    if (!config_.image_capture_enabled || info.cmd != SampleReg::GeneralOperCmd::GetCameraImage)
    {
        return;
    }

    if (!info.__isset.image || !info.image.__isset.data || info.image.data.empty())
    {
        Logger::warn("GetCameraImage callback has no image data");
        return;
    }

    if (!saveImage(info.image.width, info.image.height, info.image.data))
    {
        Logger::error("failed to save camera image");
    }
}
#endif

void CameraImageCaptureWorker::run()
{
    while (!stopping_.load())
    {
        if (camera_client_->isConnected() && !camera_client_->requestCameraImage())
        {
            Logger::warn("request camera image failed: " + camera_client_->lastError());
        }

        const auto interval_ms = config_.image_capture_interval_ms > 0 ? config_.image_capture_interval_ms : 100;
        const auto interval = std::chrono::milliseconds(interval_ms);
        const auto deadline = std::chrono::steady_clock::now() + interval;
        while (!stopping_.load() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

bool CameraImageCaptureWorker::saveImage(int width, int height, const std::string& data)
{
    if (width <= 0 || height <= 0 || data.empty())
    {
        Logger::warn("invalid camera image, width=" + std::to_string(width) +
                     ", height=" + std::to_string(height) +
                     ", bytes=" + std::to_string(data.size()));
        return false;
    }

    const auto gray_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const auto bgr_size = gray_size * 3U;

    if (data.size() == gray_size)
    {
        return saveGrayImage(makeOutputPath(".pgm"), width, height, data);
    }

    if (data.size() == bgr_size)
    {
        return saveBgrImageAsPpm(makeOutputPath(".ppm"), width, height, data);
    }

    Logger::warn("unsupported camera image byte count, saving raw data: width=" +
                 std::to_string(width) + ", height=" + std::to_string(height) +
                 ", bytes=" + std::to_string(data.size()));
    return saveRawImage(makeOutputPath(".bin"), data);
}

bool CameraImageCaptureWorker::saveGrayImage(const std::string& path,
                                             int width,
                                             int height,
                                             const std::string& data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P5\n" << width << ' ' << height << "\n255\n";
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    Logger::info("saved camera image: " + path);
    return true;
}

bool CameraImageCaptureWorker::saveBgrImageAsPpm(const std::string& path,
                                                 int width,
                                                 int height,
                                                 const std::string& data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 2 < data.size(); i += 3)
    {
        const char rgb[] = {data[i + 2], data[i + 1], data[i]};
        output.write(rgb, sizeof(rgb));
    }
    Logger::info("saved camera image: " + path);
    return true;
}

bool CameraImageCaptureWorker::saveRawImage(const std::string& path, const std::string& data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    Logger::info("saved raw camera image: " + path);
    return true;
}

std::string CameraImageCaptureWorker::makeOutputPath(const std::string& extension)
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream name;
    name << "camera_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_'
         << std::setw(6) << std::setfill('0') << image_index_.fetch_add(1)
         << extension;

    return (std::filesystem::path(config_.image_capture_dir) / name.str()).string();
}

} // namespace trackcamhub
