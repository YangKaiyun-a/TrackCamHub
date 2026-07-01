#include "workflow/CaptureResultSaver.h"

#include "app/Logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace trackcamhub
{

void CaptureResultSaver::configure(bool enabled, std::filesystem::path output_root)
{
    enabled_ = enabled;
    output_root_ = std::move(output_root);
}

#if TRACKCAMHUB_ENABLE_THRIFT
void CaptureResultSaver::saveTaskInfo(const SampleReg::TaskInfo& info)
{
    if (!enabled_)
    {
        return;
    }

    const auto paths = makeTimestampPaths();
    std::error_code ec;
    std::filesystem::create_directories(paths.directory, ec);
    if (ec)
    {
        Logger::error("failed to create camera image directory: " + paths.directory.string() + ", " + ec.message());
        return;
    }

    std::vector<std::string> image_files;
    if (!saveImages(info, paths, image_files))
    {
        Logger::warn("TaskInfoChanged contains no supported imageOut data, taskId=" + info.taskId);
    }

    if (!saveMetadata(info, paths, image_files))
    {
        Logger::error("failed to save capture result metadata, taskId=" + info.taskId);
    }
}

CaptureResultSaver::TimestampPaths CaptureResultSaver::makeTimestampPaths() const
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream date;
    date << std::put_time(&tm, "%Y%m%d");

    std::ostringstream base_name;
    base_name << date.str() << '_' << std::put_time(&tm, "%H_%M_%S");

    return TimestampPaths{output_root_ / date.str(), base_name.str()};
}

bool CaptureResultSaver::saveImages(const SampleReg::TaskInfo& info,
                                    const TimestampPaths& paths,
                                    std::vector<std::string>& image_files) const
{
    if (!info.__isset.imageOut || info.imageOut.empty())
    {
        return false;
    }

    bool saved_any = false;
    for (std::size_t i = 0; i < info.imageOut.size(); ++i)
    {
        std::ostringstream suffix;
        if (info.imageOut.size() > 1)
        {
            suffix << '_' << i;
        }

        const auto path_base = paths.directory / (paths.base_name + suffix.str());
        std::string saved_filename;
        if (saveImage(info.imageOut[i], path_base, saved_filename))
        {
            image_files.push_back(saved_filename);
            saved_any = true;
        }
    }

    return saved_any;
}

bool CaptureResultSaver::saveImage(const SampleReg::ImageInfo& image,
                                   const std::filesystem::path& path_base,
                                   std::string& saved_filename) const
{
    if (!image.__isset.data || image.width <= 0 || image.height <= 0 || image.data.empty())
    {
        return false;
    }

    const auto gray_size = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    const auto bgr_size = gray_size * 3U;

    if (image.data.size() == gray_size)
    {
        const auto path = std::filesystem::path(path_base.string() + ".pgm");
        if (saveGrayImage(path, image.width, image.height, image.data))
        {
            saved_filename = path.filename().string();
            return true;
        }
        return false;
    }

    if (image.data.size() == bgr_size)
    {
        const auto path = std::filesystem::path(path_base.string() + ".ppm");
        if (saveBgrImageAsPpm(path, image.width, image.height, image.data))
        {
            saved_filename = path.filename().string();
            return true;
        }
        return false;
    }

    const auto path = std::filesystem::path(path_base.string() + ".bin");
    if (saveRawImage(path, image.data))
    {
        saved_filename = path.filename().string();
        return true;
    }
    return false;
}

bool CaptureResultSaver::saveGrayImage(const std::filesystem::path& path,
                                       int width,
                                       int height,
                                       const std::string& data) const
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P5\n" << width << ' ' << height << "\n255\n";
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    Logger::info("saved capture image: " + path.string());
    return true;
}

bool CaptureResultSaver::saveBgrImageAsPpm(const std::filesystem::path& path,
                                           int width,
                                           int height,
                                           const std::string& data) const
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

    Logger::info("saved capture image: " + path.string());
    return true;
}

bool CaptureResultSaver::saveRawImage(const std::filesystem::path& path, const std::string& data) const
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    Logger::info("saved raw capture image: " + path.string());
    return true;
}

bool CaptureResultSaver::saveMetadata(const SampleReg::TaskInfo& info,
                                      const TimestampPaths& paths,
                                      const std::vector<std::string>& image_files) const
{
    const auto path = paths.directory / (paths.base_name + ".json");
    std::ofstream output(path);
    if (!output)
    {
        return false;
    }

    std::ostringstream result_text;
    result_text << info.result;

    output << "{\n";
    output << "  \"taskId\": \"" << jsonEscape(info.taskId) << "\",\n";
    output << "  \"state\": " << static_cast<int>(info.state) << ",\n";
    output << "  \"mode\": " << info.mode << ",\n";
    output << "  \"retCode\": " << info.retCode << ",\n";
    output << "  \"resultPresent\": " << (info.__isset.result ? "true" : "false") << ",\n";
    output << "  \"imageFiles\": [";
    for (std::size_t i = 0; i < image_files.size(); ++i)
    {
        if (i > 0)
        {
            output << ", ";
        }
        output << '"' << jsonEscape(image_files[i]) << '"';
    }
    output << "],\n";
    output << "  \"resultFlags\": " << resultFlagsJson(info.result) << ",\n";
    output << "  \"resultText\": \"" << jsonEscape(result_text.str()) << "\"\n";
    output << "}\n";

    Logger::info("saved capture metadata: " + path.string());
    return true;
}

std::string CaptureResultSaver::resultFlagsJson(const SampleReg::TaskResult& result) const
{
    std::ostringstream json;
    json << "{";
    json << "\"barcode\":" << (result.__isset.barcode ? "true" : "false") << ",";
    json << "\"bestBarcodeImage\":" << (result.__isset.bestBarcodeImage ? "true" : "false") << ",";
    json << "\"bestLiquidImage\":" << (result.__isset.bestLiquidImage ? "true" : "false") << ",";
    json << "\"tubeHeight\":" << (result.__isset.tubeHeight ? "true" : "false") << ",";
    json << "\"tubeWidth\":" << (result.__isset.tubeWidth ? "true" : "false") << ",";
    json << "\"tubeExist\":" << (result.__isset.tubeExist ? "true" : "false") << ",";
    json << "\"tubeHatColor\":" << (result.__isset.tubeHatColor ? "true" : "false") << ",";
    json << "\"tubeType\":" << (result.__isset.tubeType ? "true" : "false") << ",";
    json << "\"hatType\":" << (result.__isset.hatType ? "true" : "false") << ",";
    json << "\"tubeTilt\":" << (result.__isset.tubeTilt ? "true" : "false") << ",";
    json << "\"serumIndex\":" << (result.__isset.serumIndex ? "true" : "false") << ",";
    json << "\"sampleSize\":" << (result.__isset.sampleSize ? "true" : "false") << ",";
    json << "\"sampleCentrifuged\":" << (result.__isset.sampleCentrifuged ? "true" : "false") << ",";
    json << "\"sampleCentrifugedQuality\":" << (result.__isset.sampleCentrifugedQuality ? "true" : "false") << ",";
    json << "\"tubeOverHeadAxis\":" << (result.__isset.tubeOverHeadAxis ? "true" : "false") << ",";
    json << "\"tubeOverHead\":" << (result.__isset.tubeOverHead ? "true" : "false") << ",";
    json << "\"tubeOverHeadColor\":" << (result.__isset.tubeOverHeadColor ? "true" : "false");
    json << "}";
    return json.str();
}

std::string CaptureResultSaver::jsonEscape(const std::string& value)
{
    std::ostringstream escaped;
    for (const unsigned char ch : value)
    {
        switch (ch)
        {
        case '\\': escaped << "\\\\"; break;
        case '"': escaped << "\\\""; break;
        case '\b': escaped << "\\b"; break;
        case '\f': escaped << "\\f"; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default:
            if (ch < 0x20)
            {
                escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch);
            }
            else
            {
                escaped << static_cast<char>(ch);
            }
            break;
        }
    }
    return escaped.str();
}
#endif

} // namespace trackcamhub
