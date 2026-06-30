#pragma once

#include "config/AppConfig.h"
#include "thrift/CameraClient.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#if TRACKCAMHUB_ENABLE_THRIFT
#include "thrift/gen-cpp/SampleReg_Defs_types.h"
#endif

namespace trackcamhub
{

class CameraImageCaptureWorker
{
public:
    CameraImageCaptureWorker() = default;
    ~CameraImageCaptureWorker();

    void configure(CameraConfig config, CameraClient* camera_client);
    bool start();
    void stop();

#if TRACKCAMHUB_ENABLE_THRIFT
    void handleOperInfoChanged(const SampleReg::GeneralOperInfo& info);
#endif

private:
    void run();
    bool saveImage(int width, int height, const std::string& data);
    bool saveGrayImage(const std::string& path, int width, int height, const std::string& data);
    bool saveBgrImageAsPpm(const std::string& path, int width, int height, const std::string& data);
    bool saveRawImage(const std::string& path, const std::string& data);
    std::string makeOutputPath(const std::string& extension);

    CameraConfig config_;
    CameraClient* camera_client_ = nullptr;
    std::atomic<bool> stopping_{true};
    std::atomic<std::uint64_t> image_index_{0};
    std::thread worker_;
};

} // namespace trackcamhub
