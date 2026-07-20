# C++ 力控接口说明

当前 C++ SDK 与 Python 力控接口对齐。公开入口仍为 `#include "Codroid/client.hpp"`。

## 初始化与校准

- `ZeroForceCalibration(int calibrationTimeMs = 1000)`：零力校准 / 带载去皮。
- `InitForceControl(...)`：初始化力控。SDK 内部固定 `algo=1`（导纳），当前不允许调用方传入算法参数。`compliance` / `constantForce` / `forceLimit` 为 **`nlohmann::json`** 对象。
- `StartForceControl()` / `StopForceControl(int smoothTimeMs = 500)`：启动 / 停止力控。

`FTSensorDriftCalibration` 已废弃并移除。

## 在线参数与安全

- `TuneForceParams(...)`：在线更新刚度、阻尼、质量、期望力等。
- `StartContactDetection(...)`：接触检测。
- `SetOverforceProtection(...)`：过力保护。
- `SetForceDataHealth(...)`：力数据健康监控。

## 状态读取

`GetForceState()` 返回 `ClientForceControlState`，字段包括：

- `enabled`、`pending`、`valid`、`is_contact`、`is_overforce`：`bool`
- `algo`、`health`：`int`
- `wrench_tcp`、`wrench_base`、`desired_wrench`、`track_error`：`std::vector<double>`
- `axis_mode`：`std::vector<int>`

也可使用单字段 getter，例如 `GetForceStateEnabled()`、`GetForceStateWrenchTcp()`。

## 测试示例

见 `examples/15_force_control.cpp`：

```bash
./build_linux/15_force_control state
./build_linux/15_force_control calibration
./build_linux/15_force_control constant
./build_linux/15_force_control contact --allow-motion
```

示例中的控制器 IP 需按现场修改。
