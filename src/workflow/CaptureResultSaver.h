#pragma once

#include <filesystem>
#include <string>
#include <vector>

#if TRACKCAMHUB_ENABLE_THRIFT
#include "thrift/gen-cpp/SampleReg_Defs_types.h"
#endif

namespace trackcamhub
{

class CaptureResultSaver
{
public:
    void configure(bool enabled, std::filesystem::path output_root);

#if TRACKCAMHUB_ENABLE_THRIFT
    void saveTaskInfo(const SampleReg::TaskInfo& info);
#endif

private:
#if TRACKCAMHUB_ENABLE_THRIFT
    struct TimestampPaths
    {
        std::filesystem::path directory;
        std::string base_name;
    };

    TimestampPaths makeTimestampPaths() const;
    bool saveImages(const SampleReg::TaskInfo& info,
                    const TimestampPaths& paths,
                    std::vector<std::string>& image_files) const;
    bool saveImage(const SampleReg::ImageInfo& image,
                   const std::filesystem::path& path_base,
                   std::string& saved_filename) const;
    bool saveGrayImage(const std::filesystem::path& path,
                       int width,
                       int height,
                       const std::string& data) const;
    bool saveBgrImageAsPpm(const std::filesystem::path& path,
                           int width,
                           int height,
                           const std::string& data) const;
    bool saveRawImage(const std::filesystem::path& path, const std::string& data) const;
    bool saveMetadata(const SampleReg::TaskInfo& info,
                      const TimestampPaths& paths,
                      const std::vector<std::string>& image_files) const;
    std::string resultFlagsJson(const SampleReg::TaskResult& result) const;
    static std::string jsonEscape(const std::string& value);
#endif

    bool enabled_ = false;
    std::filesystem::path output_root_;
};

} // namespace trackcamhub
