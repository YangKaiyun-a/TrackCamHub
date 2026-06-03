namespace cpp SampleReg

include "SampleReg_Defs.thrift"

service SampleRegUC { // 上位机服务接口（下位机→上位机的通知接口）
    /**
     * 向下位机发送心跳
     * @param timeStamp 时间戳（毫秒级）
     */
    void HeartbeatToUC(1:i64 timeStamp);
    
    /**
     * 设备信息变更通知
     * @param info 变更后的设备信息
     * @return 0表示通知成功，非0表示错误码
     */
    i32 DeviceInfoChanged(1:SampleReg_Defs.DeviceInfo info);
    
    /**
     * 任务信息变更通知
     * @param info 变更后的任务信息
     * @return 0表示通知成功，非0表示错误码
     */
    i32 TaskInfoChanged(1:SampleReg_Defs.TaskInfo info);
    
    /**
     * 通用操作信息变更通知
     * @param info 变更后的通用操作信息
     * @return 0表示通知成功，非0表示错误码
     */
    i32 OperInfoChanged(1:SampleReg_Defs.GeneralOperInfo info);
}