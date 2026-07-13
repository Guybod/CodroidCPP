# C++ 力控接口说明

当前 C++ SDK 与 Python 力控接口对齐。

## 初始化与校准

- `ZeroForceCalibration(int calibrationTimeMs = 1000)`：零力校准 / 带载去皮。
- `InitForceControl(...)`：初始化力控。SDK 内部固定 `algo=1`（导纳），当前不允许调用方传入算法参数。
- `StartForceControl()` / `StopForceControl(int smoothTimeMs = 500)`：启动 / 停止力控。

`FTSensorDriftCalibration` 已废弃并移除。

## 在线参数与安全

- `TuneForceParams(...)`：在线更新刚度、阻尼、质量、期望力、力限制、坐标系和 `rampTime`。
- `StartContactDetection(...)`：接触检测。
- `SetOverforceProtection(...)`：过力保护。
- `SetForceDataHealth(...)`：力数据健康监控。

## 状态读取

`GetForceState()` 返回 `ClientForceControlState`，字段包括：

- `enabled`、`pending`、`valid`、`is_contact`、`is_overforce`：`bool`
- `algo`、`health`：`int`
- `wrench_tcp`、`wrench_base`、`desired_wrench`、`track_error`：`std::vector<double>`
- `axis_mode`：`std::vector<int>`

也可以使用单字段 getter，例如：

- `GetForceStateEnabled()` 返回 `bool`
- `GetForceStateWrenchTcp()` 返回 `std::vector<double>`
- `GetForceStateAxisMode()` 返回 `std::vector<int>`

## 测试示例

见 `examples_client/08_force_control.cpp`：

```bash
./build_linux/client_08_force_control state
./build_linux/client_08_force_control constant
./build_linux/client_08_force_control contact --allow-motion
```

示例中的控制器 IP 需要按现场修改。
