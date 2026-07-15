#include "workflow/CaptureResultSaver.h"

#include "app/Logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace trackcamhub
{

void CaptureResultSaver::configure(bool enabled,
                                   std::filesystem::path output_root,
                                   std::string camera_id,
                                   std::string serial_port)
{
    enabled_ = enabled;
    output_root_ = std::move(output_root);
    camera_id_ = std::move(camera_id);
    serial_port_ = std::move(serial_port);
}

#if TRACKCAMHUB_ENABLE_THRIFT
void CaptureResultSaver::saveTaskInfo(const SampleReg::TaskInfo& info)
{
    if (!enabled_)
    {
        return;
    }

    if (info.state != SampleReg::TaskState::Finished)
    {
        return;
    }

    const auto paths = makeTimestampPaths();
    std::error_code ec;
    std::filesystem::create_directories(paths.directory, ec);
    if (ec)
    {
        Logger::error(logContext() + "failed to create camera image directory: " +
                      paths.directory.string() + ", " + ec.message());
        return;
    }

    std::vector<std::string> image_files;
    const bool saved_result_images = saveResultImages(info, paths, image_files);
    const bool saved_image_out_images = saveImageOutImages(info, paths, image_files);
    if (!saved_result_images && !saved_image_out_images)
    {
        Logger::warn(logContext() + "TaskInfoChanged contains no supported result image data, taskId=" +
                     info.taskId);
    }

    if (!saveMetadata(info, paths, image_files))
    {
        Logger::error(logContext() + "failed to save capture result metadata, taskId=" + info.taskId);
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

    std::ostringstream directory_name;
    directory_name << std::put_time(&tm, "%Y%m%d_%H_%M_%S");

    return TimestampPaths{output_root_ / directory_name.str()};
}

bool CaptureResultSaver::saveResultImages(const SampleReg::TaskInfo& info,
                                          const TimestampPaths& paths,
                                          std::vector<std::string>& image_files) const
{
    if (!info.__isset.result)
    {
        return false;
    }

    bool saved_any = false;
    if (info.result.__isset.bestBarcodeImage &&
        info.result.bestBarcodeImage.__isset.bestBarcodeImage)
    {
        std::string saved_filename;
        if (saveImage(info.result.bestBarcodeImage.bestBarcodeImage,
                      paths.directory / "best_barcode_image",
                      saved_filename))
        {
            image_files.push_back(saved_filename);
            saved_any = true;
        }
    }

    if (info.result.__isset.bestLiquidImage &&
        info.result.bestLiquidImage.__isset.bestLiquidImage)
    {
        std::string saved_filename;
        if (saveImage(info.result.bestLiquidImage.bestLiquidImage,
                      paths.directory / "best_liquid_image",
                      saved_filename))
        {
            image_files.push_back(saved_filename);
            saved_any = true;
        }
    }

    return saved_any;
}

bool CaptureResultSaver::saveImageOutImages(const SampleReg::TaskInfo& info,
                                            const TimestampPaths& paths,
                                            std::vector<std::string>& image_files) const
{
    if (!info.__isset.imageOut || info.imageOut.empty())
    {
        return false;
    }

    const auto image_out_dir = paths.directory / "imageOut";
    std::error_code ec;
    std::filesystem::create_directories(image_out_dir, ec);
    if (ec)
    {
        Logger::error(logContext() + "failed to create imageOut directory: " +
                      image_out_dir.string() + ", " + ec.message());
        return false;
    }

    bool saved_any = false;
    for (std::size_t i = 0; i < info.imageOut.size(); ++i)
    {
        std::string saved_filename;
        const auto path_base = image_out_dir / ("imageOut_" + std::to_string(i + 1));
        if (saveImage(info.imageOut[i], path_base, saved_filename))
        {
            image_files.push_back((std::filesystem::path("imageOut") / saved_filename).string());
            saved_any = true;
        }
        else
        {
            Logger::warn(logContext() + "failed to save imageOut image, taskId=" + info.taskId +
                         ", index=" + std::to_string(i + 1));
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

    Logger::warn(logContext() + "capture image byte size does not match raw gray/BGR layout, width=" +
                 std::to_string(image.width) + ", height=" + std::to_string(image.height) +
                 ", dataBytes=" + std::to_string(image.data.size()) +
                 ", grayExpected=" + std::to_string(gray_size) +
                 ", bgrExpected=" + std::to_string(bgr_size));

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
    Logger::info(logContext() + "saved capture image: " + path.string());
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
    std::string rgb(data.size(), '\0');
    for (std::size_t i = 0; i + 2 < data.size(); i += 3)
    {
        rgb[i] = data[i + 2];
        rgb[i + 1] = data[i + 1];
        rgb[i + 2] = data[i];
    }
    output.write(rgb.data(), static_cast<std::streamsize>(rgb.size()));

    Logger::info(logContext() + "saved capture image: " + path.string());
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
    Logger::info(logContext() + "saved raw capture image: " + path.string());
    return true;
}

bool CaptureResultSaver::saveMetadata(const SampleReg::TaskInfo& info,
                                      const TimestampPaths& paths,
                                      const std::vector<std::string>& image_files) const
{
    const auto path = paths.directory / "result.json";
    std::ofstream output(path);
    if (!output)
    {
        return false;
    }

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
    output << "  \"resultFlags\": "
           << (info.__isset.result ? resultFlagsJson(info.result) : "{}") << ",\n";
    output << "  \"resultText\": \""
           << (info.__isset.result ? jsonEscape(resultTextWithoutImages(info.result)) : "") << "\"\n";
    output << "}\n";

    Logger::info(logContext() + "saved capture metadata: " + path.string());
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

std::string CaptureResultSaver::resultTextWithoutImages(const SampleReg::TaskResult& result) const
{
    auto filtered = result;
    filtered.__isset.bestBarcodeImage = false;
    filtered.__isset.bestLiquidImage = false;

    std::ostringstream text;
    text << filtered;
    return text.str();
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

std::string CaptureResultSaver::logContext() const
{
    return "[cameraId=" + camera_id_ + ", port=" + serial_port_ + "] ";
}
#endif

} // namespace trackcamhub
