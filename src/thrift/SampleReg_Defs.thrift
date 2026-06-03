namespace cpp SampleReg

// --------------1. 通用操作定义------------------------------------
/**
 * 通用操作指令枚举
 * 用于上位机对下位机发起的控制指令
 */
enum GeneralOperCmd {
    Nothing          = 0;  // 无操作
    ResetNetCfg      = 1;  // 重置网络配置（重置后自动软重启生效）
    ResetBaseCfg     = 2;  // 重置基础配置（重置后自动软重启生效）
    ResetCameraCfg   = 3;  // 重置相机配置（重置后自动软重启生效）
    ResetAllCfg      = 4;  // 重置所有配置（重置后自动软重启生效）
    UpgradeApp       = 5;  // 升级应用程序（升级后自动软重启生效）
    GetBaseInfo      = 6;  // 获取模块基础信息
    ExportLog        = 7;  // 导出最新日志
    GetCameraImage   = 8;  // 获取当前相机采集图像
    TimeSync         = 9;  // 将模块时间与上位机时间进行同步（注：断电重启后失效）
}

/**
 * 通用操作执行状态枚举
 * 用于标识操作的执行进度
 */
enum GeneralOperState {
    Nothing     = 0; // 未执行
    Issued      = 1; // 已下发（待执行）
    Doing       = 2; // 执行钟
    WaitReboot  = 3; // 等待重启生效
    Finished    = 4; // 执行完成
}

/**
 * 模块基础信息
 */
struct BaseInfo {
    1: string   modVersion;             // 模块嵌软应用版本号
    2: optional string   algVersion;    // 算法版本号（可选，无算法时可不填）
    3: optional string   modID;         // 模块标识码（可选，未设置时为空）
}

/**
 * GBR图像信息
 */
struct ImageInfo {
    1: i32    width;  // 图像宽度（像素）
    2: i32    height; // 图像高度（像素）
    3: binary data;   // 图像二进制数据（GBR格式）
}

/**
 * 通用操作信息结构体
 * 用于上位机下发操作指令及下位机返回执行结果
 */
struct GeneralOperInfo {
    1: GeneralOperCmd     cmd;                              // 操作指令（必填）
    2: optional binary    netCfgFile;                       // 网络配置文件（上位机下发时使用）
    3: optional binary    baseCfgFile;                      // 基础配置文件（上位机下发时使用）
    4: optional binary    cameraCfgFile;                    // 相机配置文件（上位机下发时使用）
    5: optional binary    upgradePack;                      // 升级包数据（上位机升级时使用）
    6: optional BaseInfo  baseInfo;                         // 模块基础信息（下位机返回时使用）
    7: optional binary    modLog;                           // 模块日志数据（下位机返回时使用）
    8: optional ImageInfo image;                            // 相机图像数据（下位机返回时使用）
    9: optional string    time;                             // 上位机时间（上位机发送当前时间如"2025-11-19 19:38:50"）
    10: GeneralOperState  state = GeneralOperState.Nothing; // 操作执行状态（默认未执行）
    11: optional i32      retCode;                          // 返回码：0-成功，非0-错误码
}

/**
 * 设备运行状态枚举
 */
enum RunningState {
    Ready    = 0; // 就绪（正常运行）
    Reseting = 1; // 复位中（重启过程）
    Stop     = 2; // 停机（主动停止）
    Fault    = 3; // 故障（异常状态）
}

/**
 * 设备信息结构体
 * 包含设备当前的运行状态
 */
struct DeviceInfo {
    1: RunningState runningState; // 设备运行状态（必填）
}

//-------------- 2. 视觉识别任务定义---------------------------------------------------------

/**
 * 样本管管帽颜色枚举
 */
enum TubeHatColor {
    TUBE_HAT_RED;        // 红色
    TUBE_HAT_PINK;       // 粉色
    TUBE_HAT_BLUE;       // 蓝色
    TUBE_HAT_BLACK;      // 黑色
    TUBE_HAT_GREEN;      // 绿色
    TUBE_HAT_PURPLE;     // 紫色
    TUBE_HAT_GRAY;       // 灰色
    TUBE_HAT_ORANGE;     // 橙色
    TUBE_HAT_YELLOW;     // 黄色
    TUBE_HAT_WHITE;      // 白色
    TUBE_HAT_UNKNOWN;    // 无法识别
    TUBE_HAT_NONE;       // 无试管帽
}

/**
 * 试管类型枚举
 */
enum TubeType {
    HITACHI_MICRO_TUBE;     // 日立微量杯
    UNKNOWN_TUBE;           // 无法识别类型
    QUALITY_CONTROL_TUBE;   // 质控品管
    HITACHI_STANDARD_TUBE;  // 日立标准杯   
    BLOOD_COLLECTION_TUBE;  // 采血管
    BLANK;                  // 空白
}

/**
 * 俯拍试管状态枚举
 */
enum TubeOverHeadStatus {
    UNKNOWN = -1;   // 未知状态
    NO_TUBE = 0;    // 无试管
    NOHAT_TUBE = 1; // 无帽试管
    HAT_TUBE = 2;   // 有帽试管
}

/**
 * 管帽类型
 */
enum HatType
{
    ONE_TIME_USE;      // 一次性管帽
    NORMAL;            // 常规管帽
    NONE_HAT;          // 无管帽
}

/**
 * 二维坐标矩形框结构体
 */
struct Bbox {
    1: i32 x;   // 左上角x坐标（像素）
    2: i32 y;   // 左上角y坐标（像素）
    3: i32 w;   // 宽度（像素）
    4: i32 h;   // 高度（像素）
}

/**
 * 样本管管帽颜色HSV值结构体
 */
struct TubeHatHSV {
    1: i8 H;  // 色相
    2: i8 S;  // 饱和度
    3: i8 V;  // 明度
}

/**
 * 条码信息结构体
 */
struct BarcodeInfo {
    1: optional list<string> barcodes;       // 识别到的条码列表（多个条码时按优先级排序）
    2: i32 status = -1;                      // 状态码（-1：识别失败，0：成功）
}

/**
 * 最佳条码图像信息结构体
 */
struct BestBarcodeImageInfo {
    1: optional ImageInfo bestBarcodeImage;   // 最佳条码图像
    2: i32 status = -1;                       // 状态码（-1：获取失败，0：成功）
}

/**
 * 最佳液面图像信息结构体
 */
struct BestLiquidImageInfo {
    1: optional ImageInfo bestLiquidImage;  // 最佳液面图像
    2: i32 status = -1;                     // 状态码（-1：获取失败，0：成功）
}

/**
 * 试管高度信息结构体
 */
struct TubeHeightInfo {
    1: optional double height;             // 实测高度（单位：mm）
    2: optional i32 suspectHeight;         // 疑似高度（75或者100mm）
    3: i32 status = -1;                    // 状态码（-1：检测失败，0：成功）
}

/**
 * 试管宽度信息结构体
 */
struct TubeWidthInfo {
    1: optional double width;              // 实测宽度（单位：mm）
    2: optional i32 suspectWidth ;         // 疑似宽度（10或者13mm）
    3: i32 status = -1;                    // 状态码（-1：检测失败，0：成功）
}

/**
 * 试管有无信息结构体
 */
struct TubeExistInfo {
    1: optional bool isExist;          // 试管有无（true：有，false：无）
    2: i32 status = -1;                // 状态码（-1：检测失败，0：成功）
}

/**
 * 试管管帽颜色信息结构体
 */
struct TubeHatColorInfo {
    1: optional TubeHatColor tubeHatColor;  // 管帽颜色枚举值
    2: optional TubeHatHSV tubeHatHsv;      // 管帽HSV值
    3: i32 status = -1;                     // 状态码（-1：检测失败，0：成功）
}

/**
 * 试管类型信息结构体
 */
struct TubeTypeInfo {
    1: optional TubeType tubeType;  // 试管类型枚举值
    2: i32 status = -1;             // 状态码（-1：检测失败，0：成功）
}

// 管帽类型
struct HatTypeInfo
{
    1: optional HatType  hatType;    // 管帽类型
    2: i32      status   = -1;       // 状态码（-1：检测失败，0：成功）
}

/**
 * 试管倾斜信息结构体
 */
struct TubeTiltInfo {
    1: optional double tubeBodyTilt;                // 试管主体倾斜角度（单位：度）
    2: optional double tubeHatTilt;                 // 试管帽倾斜角度（单位：度）
    3: optional double tubeHatRelativeToBodyTilt;   // 管帽相对主体倾斜角度（单位：度）
    4: i32 status = -1;                             // 状态码（-1：检测失败，0：成功）
}

/**
 * 血清指数信息结构体
 */
struct SerumIndexInfo {
    1: optional double hemolysisLevel;  // 溶血等级
    2: optional double icterusLevel;    // 黄疸等级
    3: optional double lipemiaLevel;    // 脂血等级
    4: i32 status = -1;                 // 状态码（-1：检测失败，0：成功）
}

/**
 * 离心后样本分层高度结构体（单位：mm，距离管底高度）
 */
struct SampleHeight {
    1: double serumHeight;      // 血清层高度
    2: double gelHeight;        // 凝胶层高度
    3: double cellsHeight;      // 红细胞高度
    4: double allSampleHeight;  // 整体样本高度
}

/**
 * 离心后样本分层体积结构体（单位：mL）
 */
struct SampleVolume {
    1: double serumVolume;     // 血清层体积
    2: double gelVolume;       // 凝胶层体积
    3: double cellsVolume;     // 红细胞体积
    4: double allSampleVolume; // 整体样本体积
}

/**
 * 样本量信息结构体
 */
struct SampleSizeInfo {
    1: optional SampleHeight height;   // 分层高度信息
    2: optional SampleVolume volume;   // 分层体积信息
    3: i32 status = -1;                // 状态码（-1：检测失败，0：成功）
}

/**
 * 离心状态信息结构体
 */
struct SampleCentrifugedInfo {
    1: optional bool isCentrifuged;         // 是否离心（true：已离心，false：未离心）
    2: i32 status = -1;                     // 状态码（-1：检测失败，0：成功）
}

/**
 * 离心质量判定信息结构体
 */
struct SampleCentrifugedQualityInfo {
    1: optional bool hasBubble;          // 是否有气泡
    2: optional bool hasGrume;           // 是否有凝块
    3: optional bool hasFibers;          // 是否有纤维
    4: i32 status = -1;                  // 状态码（-1：检测失败，0：成功）
}

/**
 * 俯拍试管坐标信息结构体
 */
struct TubeOverHeadAxisInfo {
    1: optional list<Bbox> tubeAxis;        // 试管坐标
    2: i32 status = -1;                     // 状态码（-1：检测失败，0：成功）
}

/**
 * 俯拍试管有无信息结构体
 */
struct TubeOverHeadInfo {
    1: optional list<TubeOverHeadStatus> info = [];  // 俯拍试管有无列表（与坐标列表一一对应）
    2: i32 status = -1;                              // 状态码（-1：检测失败，0：成功）
}

/**
 * 俯拍管帽颜色信息结构体
 */
struct TubeOverHeadColor {
    1: optional list<TubeHatColorInfo> colors = [];   // 俯拍管帽颜色列表（与坐标列表一一对应）
    2: i32 status = -1;                               // 状态码（-1：检测失败，0：成功）
}

/**
 * 任务类型枚举
 * 对应视觉识别的具体功能
 */
enum TaskType {
    TASK_GET_BARCODE = 0;                          // 获取条码信息
    TASK_GET_BEST_BARCODE_IMAGE = 1;               // 获取最佳条码图像
    TASK_GET_BEST_LIQUID_IMAGE = 2;                // 获取最佳液面图像
    TASK_GET_TUBE_HEIGHT = 3;                      // 获取试管高度
    TASK_GET_TUBE_WIDTH = 4;                       // 获取试管宽度
    TASK_GET_TUBE_EXIST = 5;                       // 获取试管有无
    TASK_GET_TUBE_HAT_COLOR = 6;                   // 获取试管管帽颜色
    TASK_GET_TUBE_TYPE = 7;                        // 获取试管类型
    TASK_GET_HAT_TYPE = 8;                         // 获取管帽类型
    TASK_GET_TUBE_TILT = 9;                        // 获取试管倾斜角度
    TASK_GET_SERUM_INDEX = 10;                     // 获取血清指数
    TASK_GET_SAMPLE_SIZE = 11;                     // 获取样本量信息
    TASK_GET_SAMPLE_CENTRIFUGED = 12;              // 获取离心状态
    TASK_GET_SAMPLE_CENTRIFUGED_QUALITY = 13;      // 获取离心质量
    TASK_GET_TUBE_OVERHEAD_AXIS = 14;              // 获取俯拍试管坐标
    TASK_GET_TUBE_OVERHEAD = 15;                   // 获取俯拍试管有无
    TASK_GET_TUBE_OVERHEAD_COLOR = 16;             // 获取俯拍管帽颜色
}

/**
 * 任务执行状态枚举
 */
enum TaskState {
    Nothing     = 0; // 未执行
    Issued      = 1; // 已下发（待执行）
    Identifying = 2; // 识别中
    Finished    = 3; // 已完成
}

/**
 * 任务执行结果结构体
 * 根据任务类型返回对应字段（其他字段为null）
 */
struct TaskResult {
    1: optional BarcodeInfo barcode;                                    // 条码信息（对应TASK_GET_BARCODE）
    2: optional BestBarcodeImageInfo bestBarcodeImage;                  // 最佳条码图像（对应TASK_GET_BEST_BARCODE_IMAGE）
    3: optional BestLiquidImageInfo bestLiquidImage;                    // 最佳液面图像（对应TASK_GET_BEST_LIQUID_IMAGE）
    4: optional TubeHeightInfo tubeHeight;                              // 试管高度（对应TASK_GET_TUBE_HEIGHT）
    5: optional TubeWidthInfo tubeWidth;                                // 试管宽度（对应TASK_GET_TUBE_WIDTH）
    6: optional TubeExistInfo tubeExist;                                // 试管有无（对应TASK_GET_TUBE_EXIST）
    7: optional TubeHatColorInfo tubeHatColor;                          // 管帽颜色（对应TASK_GET_TUBE_HAT_COLOR）
    8: optional TubeTypeInfo tubeType;                                  // 试管类型（对应TASK_GET_TUBE_TYPE）
    9: optional HatTypeInfo hatType;                                    // 管帽类型（对应TASK_GET_HAT_TYPE）
    10: optional TubeTiltInfo tubeTilt;                                 // 试管倾斜（对应TASK_GET_TUBE_TILT）
    11: optional SerumIndexInfo serumIndex;                             // 血清指数（对应TASK_GET_SERUM_INDEX）
    12: optional SampleSizeInfo sampleSize;                             // 样本量信息（对应TASK_GET_SAMPLE_SIZE）
    13: optional SampleCentrifugedInfo sampleCentrifuged;               // 离心状态（对应TASK_GET_SAMPLE_CENTRIFUGED）
    14: optional SampleCentrifugedQualityInfo sampleCentrifugedQuality; // 离心质量（对应TASK_GET_SAMPLE_CENTRIFUGED_QUALITY）
    15: optional TubeOverHeadAxisInfo tubeOverHeadAxis;                 // 俯拍坐标（对应TASK_GET_TUBE_OVERHEAD_AXIS）
    16: optional TubeOverHeadInfo tubeOverHead;                         // 俯拍有无（对应TASK_GET_TUBE_OVERHEAD）
    17: optional TubeOverHeadColor tubeOverHeadColor;                   // 俯拍颜色（对应TASK_GET_TUBE_OVERHEAD_COLOR）
}

/**
 * 任务信息结构体
 * 包含任务描述、状态及执行结果
 */
struct TaskInfo {
    1: string taskId;                                  // 任务唯一ID（必填）
    2: list<TaskType> taskType;                        // 任务类型列表（支持批量任务）
    3: TaskState state = TaskState.Nothing;            // 任务执行状态（默认未执行）
    4: i32 mode;                                       // 识别模式：0-用输入图像识别，1-拍单张识别，2-拍多张（整圈）识别
    5: optional list<ImageInfo> imageIn;               // 输入图像（mode=0时必填）
    6: optional i32 retCode;                           // 返回码：0-成功，非0-错误码
    7: optional list<ImageInfo> imageOut;              // 输出图像（处理后的结果图像）
    8: optional TaskResult result;                     // 识别结果（任务完成后有效）
}