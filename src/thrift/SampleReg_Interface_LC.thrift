namespace cpp SampleReg

include "SampleReg_Defs.thrift"

service SampleRegLC { // 下位机服务接口（上位机→下位机的控制接口）
    /**
     * 向上位机发送心跳
     * @param timeStamp 时间戳（毫秒级）
     */
    void HeartbeatToLC(1:i64 timeStamp);
    
    /**
     * 发布通用操作指令
     * @param info 通用操作信息（包含指令及参数）
     * @return 0表示下发成功，非0表示错误码
     */
    i32 DistributeOper(1:SampleReg_Defs.GeneralOperInfo info);
    
    /**
     * 发布任务指令
     * @param info 任务信息（包含任务ID、类型等）
     * @return 0表示下发成功，非0表示错误码
     */
    i32 DistributeTask(1:SampleReg_Defs.TaskInfo info);
    
    /**
     * 获取当前任务信息
     * @return 任务详细信息
     */
    SampleReg_Defs.TaskInfo GetTaskInfo();
    
    /**
     * 获取当前设备信息
     * @return 设备运行状态等信息
     */
    SampleReg_Defs.DeviceInfo GetDeviceInfo();
    
    /**
     * 获取当前通用操作信息
     * @return 通用操作的执行状态及结果
     */
    SampleReg_Defs.GeneralOperInfo GetOperInfo();
}