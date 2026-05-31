# CodroidClient API 参考

**类:** `CodroidClient`
**命名空间:** `Codroid`
**头文件:** `#include "Codroid/client.hpp"`

`CodroidClient` 是 SDK 的主入口类，提供连接、IO、寄存器、运动、CRI、主题订阅等功能。

```cpp
#include "Codroid/client.hpp"

Codroid::CodroidClient robot;
```

---

## 通用类型

### CommandResult

大多数 TCP 指令方法的返回类型。

```cpp
struct CommandResult {
    int id;               // 请求 id（与下发一致）
    std::string ty;       // 响应类型 / 路由
    std::string error_msg; // 控制器 err 或本地错误说明；空表示成功
    std::string raw_json;  // 最近一次完整响应 JSON

    bool Ok() const noexcept; // error_msg 为空返回 true
};
```

| 属性/方法 | 类型 | 说明 |
|-----------|------|------|
| `id` | `int` | 请求 ID |
| `ty` | `std::string` | 响应类型标识 |
| `error_msg` | `std::string` | 非空表示失败 |
| `raw_json` | `std::string` | 完整响应 JSON |
| `Ok()` | `bool` | 是否成功（`error_msg` 为空） |

### MotionWaitOptions

阻塞运动等待参数，用于 `*Sync` 方法。

```cpp
struct MotionWaitOptions {
    double timeout_s = 60.0;                        // 整体等待超时（秒）
    double poll_interval_s = 0.05;                  // CRI 轮询间隔（秒）
    double cri_stale_timeout_s = 0.5;               // CRI 数据过期判定（秒）
    int settled_samples = 3;                        // InMotion=false 连续稳定采样数
    double joint_tolerance_deg = 0.2;               // 关节目标容差（度）
    double cartesian_position_tolerance_mm = 1.0;   // 笛卡尔位置容差（mm）
    double cartesian_orientation_tolerance_deg = 1.0; // 笛卡尔姿态容差（度）
};
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `timeout_s` | `double` | 60.0 | 整体等待超时（秒） |
| `poll_interval_s` | `double` | 0.05 | CRI 轮询间隔（秒） |
| `cri_stale_timeout_s` | `double` | 0.5 | CRI 数据过期判定（秒） |
| `settled_samples` | `int` | 3 | InMotion=false 连续稳定采样数 |
| `joint_tolerance_deg` | `double` | 0.2 | 关节目标容差（度） |
| `cartesian_position_tolerance_mm` | `double` | 1.0 | 笛卡尔位置容差（mm） |
| `cartesian_orientation_tolerance_deg` | `double` | 1.0 | 笛卡尔姿态容差（度） |

### 异常类型

| 异常 | 触发条件 | 来源 |
|------|----------|------|
| `CodroidCommandException` | 控制器返回 `err` 字段 | TCP 响应 |
| `CodroidException` | 通用运行时错误 | SDK 内部 |
| `std::runtime_error` | Sync 方法在异常状态或超时时 | 阻塞运动 |

#### CodroidCommandException

```cpp
class CodroidCommandException : public CodroidException {
public:
    int request_id() const noexcept;
    const std::string& command_ty() const noexcept;
    const std::string& controller_error() const noexcept;
    const std::string& raw_response_json() const noexcept;
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `request_id()` | `int` | 协议请求 ID |
| `command_ty()` | `std::string` | 命令类型，如 `"Robot/move"` |
| `controller_error()` | `std::string` | 控制器返回的错误描述 |
| `raw_response_json()` | `std::string` | 完整响应 JSON |

---

## 构造与析构

### CodroidClient()

默认构造函数，不建立连接。

```cpp
Codroid::CodroidClient robot;
```

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参构造 |

**返回值：** 无

**异常：** 无

---

### ~CodroidClient()

析构函数。如果未调用 `Disconnect()`，析构时会自动清理资源。

**返回值：** 无

**异常：** 无

---

## 连接管理

### Connect

```cpp
bool Connect(const std::string& ip, int port = 9001);
```

建立 TCP 连接，不切模式、不自动上电。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ip` | `const std::string&` | -- | 控制器 IP 地址 |
| `port` | `int` | 9001 | TCP 端口 |

**返回值：** `bool` -- 连接成功返回 `true`

**异常：** 无（失败时返回 `false`）

```cpp
if (!robot.Connect("192.168.8.136")) {
    std::cerr << "连接失败" << std::endl;
}
```

---

### ConnectRemoteAndSwitchOn

```cpp
bool ConnectRemoteAndSwitchOn(const std::string& ip, int port = 9001, std::string local_ip = {});
```

连接并执行远程上电/模式切换。推荐的一键初始化方法。`local_ip` 非空时用于 `StartCriDataPush` 绑定本机 UDP。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ip` | `const std::string&` | -- | 控制器 IP 地址 |
| `port` | `int` | 9001 | TCP 端口 |
| `local_ip` | `std::string` | `{}` (空) | 本机 IP，用于 CRI UDP 绑定；非空时自动启动 CRI 数据推送 |

**返回值：** `bool` -- 连接、切换远程、上电均成功返回 `true`

**异常：** 无（失败时返回 `false`）

```cpp
robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150");
```

---

### Disconnect

```cpp
void Disconnect();
```

断开 TCP 连接，停止 CRI 相关线程与缓存。始终在程序结束前调用。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** 无

**异常：** 无

```cpp
try {
    robot.ConnectRemoteAndSwitchOn("192.168.8.136");
    // ... 操作 ...
} catch (...) {
    robot.Disconnect();
    throw;
}
robot.Disconnect();
```

---

### NextRequestId

```cpp
int NextRequestId();
```

生成下一个单调递增的请求 ID。多线程发令时避免重复。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `int` -- 下一个可用的请求 ID

**异常：** 无

---

## 模式控制

### SwitchOn

```cpp
CommandResult SwitchOn(int id = 1);
```

上电使能。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 通过 `Ok()` 判断是否成功，`error_msg` 非空表示失败

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SwitchOff

```cpp
CommandResult SwitchOff(int id = 1);
```

下电。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToManual

```cpp
CommandResult ToManual(int id = 1);
```

切换到手动模式。需要固件 2.3.3.43+。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToAuto

```cpp
CommandResult ToAuto(int id = 1);
```

切换到自动模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToRemote

```cpp
CommandResult ToRemote(int id = 1);
```

切换到远程模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ClearSystemError

```cpp
CommandResult ClearSystemError(int id = 1);
```

清除系统错误状态。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### EnterManualModeViaAuto

```cpp
CommandResult EnterManualModeViaAuto(int id = 1);
```

先切自动再切手动（Auto -> Manual）。满足控制器"必须经过自动模式"的限制。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### EnterRemoteModeViaAuto

```cpp
CommandResult EnterRemoteModeViaAuto(int id = 1);
```

先切自动再切远程（Auto -> Remote）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToSimulation

```cpp
CommandResult ToSimulation(int id = 1);
```

进入仿真模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToActual

```cpp
CommandResult ToActual(int id = 1);
```

进入实机模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StartDrag

```cpp
CommandResult StartDrag(int id = 1);
```

进入拖拽示教模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopDrag

```cpp
CommandResult StopDrag(int id = 1);
```

退出拖拽示教模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## IO 操作

### GetDi

```cpp
int GetDi(int port, int id = 1);
```

读取数字量输入。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `int` -- DI 值（0 或 1）

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
int di0 = robot.GetDi(0);
std::cout << "DI 0 = " << di0 << std::endl;
```

---

### GetDo

```cpp
int GetDo(int port, int id = 1);
```

读取数字量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `int` -- DO 值（0 或 1）

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### GetAi

```cpp
double GetAi(int port, int id = 1);
```

读取模拟量输入。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `double` -- AI 值

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### GetAo

```cpp
double GetAo(int port, int id = 1);
```

读取模拟量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `double` -- AO 值

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetDo

```cpp
CommandResult SetDo(int port, int value, int id = 1);
```

设置数字量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `value` | `int` | -- | 写入值（0 或 1） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetDo(10, 1);  // 设置 DO 10 为高
robot.SetDo(10, 0);  // 设置 DO 10 为低
```

---

### SetAo

```cpp
CommandResult SetAo(int port, double value, int id = 1);
```

设置模拟量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `value` | `double` | -- | 写入值 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetAo(2, 3.14);
```

---

## 寄存器操作

### GetRegisterValue

```cpp
double GetRegisterValue(int address, int id = 1);
```

读取单个寄存器值。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `address` | `int` | -- | 寄存器地址 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `double` -- 寄存器值

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
double val = robot.GetRegisterValue(49100);
std::cout << "Register 49100 = " << val << std::endl;
```

---

### GetRegisterValues

```cpp
std::vector<ClientRegisterInfo> GetRegisterValues(const std::vector<int>& addresses, int id = 1);
```

批量读取寄存器。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `addresses` | `const std::vector<int>&` | -- | 寄存器地址列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<ClientRegisterInfo>` -- 寄存器值列表，每个元素包含 `address` 和 `value`

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
auto regs = robot.GetRegisterValues({49100, 49101, 49102});
for (const auto& r : regs) {
    std::cout << "Register " << r.address << " = " << r.value << std::endl;
}
```

**ClientRegisterInfo 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `address` | `int` | 寄存器地址 |
| `value` | `double` | 寄存器值 |

---

### SetRegisterValue

```cpp
CommandResult SetRegisterValue(int address, double value, int id = 1);
```

设置寄存器值。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `address` | `int` | -- | 寄存器地址 |
| `value` | `double` | -- | 写入值 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetRegisterValue(49100, 42);
robot.SetRegisterValue(49300, 3.14);
```

---

## 运动控制

### MovJ -- 关节运动

```cpp
CommandResult MovJ(const ClientJointPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovJ(const ClientCartesianPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovJ(const ClientMovePoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

关节运动。发送指令后立即返回。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientJointPoint` / `ClientCartesianPoint` / `ClientMovePoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（关节 deg/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥 -- 同时传入时 `relativeBlend` 无效。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥 -- 同时传入时此参数无效。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 [x,y,z,a,b,c]。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
// 关节目标
robot.MovJ(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100);

// 笛卡尔目标（关节运动到 TCP 位姿，控制器做逆解）
robot.MovJ(Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}), 40, 100);
```

---

### MovL -- 直线运动

```cpp
CommandResult MovL(const ClientCartesianPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovL(const ClientJointPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovL(const ClientMovePoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

直线运动。发送指令后立即返回。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientCartesianPoint` / `ClientJointPoint` / `ClientMovePoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
auto state = robot.GetRobotRealtimeState();
robot.MovL(
    Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
    150, 500);
```

---

### MovC -- 圆弧运动

```cpp
CommandResult MovC(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

圆弧运动。经过中间点到达目标点。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点（圆弧上） |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.MovC(
    Codroid::CartesianPoint::MmDeg({450, 100, 300, 180, 0, 0}),
    Codroid::CartesianPoint::MmDeg({500, 0, 300, 180, 0, 0}),
    100, 300);
```

---

### MovCircle -- 整圆运动

```cpp
CommandResult MovCircle(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                        int circle_num, double speed, double acceleration,
                        double blend = -1, double relativeBlend = -1,
                        const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

整圆/多圈运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点 |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `circle_num` | `int` | -- | 整圆圈数 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.MovCircle(
    Codroid::CartesianPoint::MmDeg(mid),
    Codroid::CartesianPoint::MmDeg(end),
    1, 80, 200);
```

---

### Move / MovePath -- 多段路径

```cpp
CommandResult Move(const std::vector<ClientMoveInstruction>& path, int id = 1);
CommandResult MovePath(const std::vector<ClientMoveInstruction>& path, int id = 1);
```

将一组运动指令作为单条路径命令发送。`MovePath` 是 `Move` 的别名。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `path` | `const std::vector<ClientMoveInstruction>&` | -- | 运动指令列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(
        Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100),
    Codroid::ClientMoveInstruction::MovL(
        Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}), 150, 500)
};
robot.Move(path);
```

---

### PauseRobotMotion

```cpp
CommandResult PauseRobotMotion(int id = 1);
```

暂停当前运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ResumeRobotMotion

```cpp
CommandResult ResumeRobotMotion(int id = 1);
```

恢复暂停的运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopRobotMove

```cpp
CommandResult StopRobotMove(int id = 1);
```

立即停止当前运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## 阻塞运动（Sync）

`*Sync` 方法发送运动指令后**阻塞直到 CRI 确认机器人到达目标**。成功返回 `true`，错误/超时抛出 `std::runtime_error`。

**必须先调用** `StartCriDataPush` 并 `WaitForCriData` 等待首帧。

### MoveSync

```cpp
bool MoveSync(const std::vector<ClientMoveInstruction>& path, const MotionWaitOptions& wait = {});
```

发送多段路径并阻塞直到最后一段目标到达。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `path` | `const std::vector<ClientMoveInstruction>&` | -- | 运动指令列表 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项（超时、容差等） |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或机器人处于异常状态（碰撞、急停、报警）

```cpp
std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(Codroid::JointPoint::Degrees({0,0,90,0,90,0}), 40, 100),
    Codroid::ClientMoveInstruction::MovL(Codroid::CartesianPoint::MmDeg({400,0,300,180,0,0}), 150, 500),
};
robot.MoveSync(path);
```

---

### MovJSync

```cpp
bool MovJSync(const ClientJointPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});

bool MovJSync(const ClientCartesianPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式关节运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientJointPoint` / `ClientCartesianPoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（deg/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

```cpp
Codroid::MotionWaitOptions wait;
wait.timeout_s = 90.0;
wait.joint_tolerance_deg = 0.3;
robot.MovJSync(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100, wait);
```

---

### MovLSync

```cpp
bool MovLSync(const ClientCartesianPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});

bool MovLSync(const ClientJointPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式直线运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientCartesianPoint` / `ClientJointPoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

```cpp
auto state = robot.GetRobotRealtimeState();
Codroid::MotionWaitOptions wait;
wait.timeout_s = 60.0;
wait.cartesian_position_tolerance_mm = 2.0;
wait.cartesian_orientation_tolerance_deg = 1.5;
robot.MovLSync(
    Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
    150, 500, wait);
```

---

### MovCSync

```cpp
bool MovCSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
              double speed, double acc, const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式圆弧运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点（圆弧上） |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

```cpp
robot.MovCSync(
    Codroid::CartesianPoint::MmDeg(mid),
    Codroid::CartesianPoint::MmDeg(end),
    100, 300);
```

---

### MovCircleSync

```cpp
bool MovCircleSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   int circle_num, double speed, double acc, const MotionWaitOptions& wait = {},
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式整圆运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点 |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `circle_num` | `int` | -- | 整圆圈数 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

---

## MoveTo（规划运动）

### MoveTo

```cpp
CommandResult MoveTo(const MoveToParams& params, int id = 1);
```

MoveTo 预设/规划运动。运行期间需要心跳。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const MoveToParams&` | -- | MoveTo 参数（类型 + 可选目标） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**MoveToParams 结构：**

```cpp
struct MoveToParams {
    MoveToType type = MoveToType::Home;  // 运动类型
    MoveToTarget target;                 // 目标点（仅 Joint/Line 需要）
};
```

**MoveToType 枚举：**

| 名称 | 值 | 说明 |
|------|-----|------|
| `Stop` | -1 | 停止 MoveTo |
| `Home` | 0 | 回 Home |
| `Safe` | 1 | 回安全位 |
| `Candle` | 2 | 回 Candle 位 |
| `Packing` | 3 | 回 Packing 位 |
| `Joint` | 4 | 关节规划到目标 |
| `Line` | 5 | 直线规划到目标 |
| `ResumePoint` | 6 | 恢复程序执行 |

```cpp
// 回 Home
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Home));

// 关节规划到目标
auto target = Codroid::MoveToTarget::Joint(
    Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}));
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Joint, target));
```

---

### MoveToHeartbeat

```cpp
CommandResult MoveToHeartbeat(int id = 1);
```

MoveTo 心跳。使用 `MoveToType::Joint` 或 `Line` 时，每 >=500ms 调用一次，否则运动会停止。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopMoveTo

```cpp
CommandResult StopMoveTo(int id = 1);
```

停止 MoveTo 运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## Jog（点动）

### Jog

```cpp
CommandResult Jog(const JogParams& params, int id = 1);
```

点动。需要约每 500ms 发送心跳维持。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const JogParams&` | -- | 点动参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**JogParams 结构：**

```cpp
struct JogParams {
    JogMode mode = JogMode::Line;       // Joint=1, Line=2
    double speed = 0.0;                 // -1~1 的比例
    int index = 1;                      // 轴号（Joint 模式）或方向（Line 模式）
    CoorType coorType = CoorType::User; // 坐标系类型
    int coorId = 1;                     // 坐标系 ID
};
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `mode` | `JogMode` | `Line` | 点动模式：`Joint`(1) 或 `Line`(2) |
| `speed` | `double` | 0.0 | 速度比例（-1~1） |
| `index` | `int` | 1 | 轴号（Joint 模式 1--6）或方向（Line 模式） |
| `coorType` | `CoorType` | `User` | 坐标系类型：`User` 或 `Tool` |
| `coorId` | `int` | 1 | 坐标系 ID |

```cpp
// 关节 1 正向点动
robot.Jog(Codroid::JogParams(Codroid::JogMode::Joint, 0.5, 1));

// 笛卡尔 X 正向点动
robot.Jog(Codroid::JogParams(Codroid::JogMode::Line, 0.3, 1));
```

---

### StopJog

```cpp
CommandResult StopJog(int id = 1);
```

停止点动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### JogHeartbeat

```cpp
CommandResult JogHeartbeat(int id = 1);
```

点动心跳。点动期间每 >=500ms 调用一次。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## CRI 实时数据

### StartCriDataPush

```cpp
CommandResult StartCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

请求 CRI 状态 UDP 推送。固定参数：100ms 周期、高精度、mask 0xFFFF、308 字节 UDP 包。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `udpIp` | `const std::string&` | -- | 本机 IP 地址，用于接收 UDP 数据 |
| `udpPort` | `int` | -- | 本机 UDP 端口 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.StartCriDataPush("192.168.8.150", 18888);
robot.WaitForCriData(5.0); // 等待首帧
```

---

### StopCriDataPush

```cpp
CommandResult StopCriDataPush(int id = 1);
CommandResult StopCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

停止 CRI 数据推送。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `udpIp` | `const std::string&` | -- | 本机 IP（带参版本） |
| `udpPort` | `int` | -- | 本机 UDP 端口（带参版本） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.StopCriDataPush("192.168.8.150", 18888);
```

---

### StartCriControl

```cpp
CommandResult StartCriControl(int filterType, int durationMs, int startBuffer, int id = 1);
```

启动实时控制会话。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `filterType` | `int` | -- | 滤波类型（0=关闭，1=均值，2=二阶低通，3=椭圆） |
| `durationMs` | `int` | -- | 控制周期（ms），须与 `CriRealtimeDispatcher::SendTrajectory` 的 period 一致 |
| `startBuffer` | `int` | -- | 起始缓冲帧数（1~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.StartCriControl(1, 4, 5);  // 均值滤波, 4ms 周期, 5 帧缓冲
```

---

### StopCriControl

```cpp
CommandResult StopCriControl(int id = 1);
```

停止实时控制会话。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### GetCriUdpListenPort

```cpp
int GetCriUdpListenPort() const;
```

获取 CRI UDP 监听端口。未启动推送时可能为 0。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `int` -- 当前 CRI UDP 监听端口

**异常：** 无

---

### GetRobotRealtimeState

```cpp
ClientRealtimeState GetRobotRealtimeState() const;
```

获取线程安全的 CRI 数据快照。须先 `StartCriDataPush`，首帧到达前 `data_valid` 可能为 false。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `ClientRealtimeState` -- 实时状态快照

**异常：** 无

**ClientRealtimeState 关键字段：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `timestamp_ms` | `int64_t` | 控制器端时间戳（ms） |
| `data_valid` | `bool` | 数据是否有效 |
| `joint_position` | `std::vector<double>` | 关节位置（度），六轴 |
| `joint_velocity` | `std::vector<double>` | 关节角速度（deg/s） |
| `tcp_pose` | `std::vector<double>` | [x,y,z,rx,ry,rz]，mm + 度 |
| `tcp_velocity` | `std::vector<double>` | 线 mm/s，角 deg/s |
| `tcp_linear_velocity_mm_s` | `double` | TCP 线速度标量 |
| `joint_output_torque` | `std::vector<double>` | 关节输出力矩 |
| `joint_external_force` | `std::vector<double>` | 关节外部力 |
| `in_motion` | `bool` | 是否正在运动 |
| `has_alarm` | `bool` | 是否有报警 |
| `collision_stopped` | `bool` | 是否因碰撞停止 |
| `emergency_stop_pressed` | `bool` | 急停是否按下 |
| `remote_mode` | `bool` | 是否远程模式 |
| `auto_mode` | `bool` | 是否自动模式 |
| `manual_mode` | `bool` | 是否手动模式 |
| `realtime_control_mode` | `bool` | 是否实时控制模式 |
| `project_running` | `bool` | 工程是否运行中 |

```cpp
auto state = robot.GetRobotRealtimeState();
if (state.data_valid) {
    std::cout << "J1 = " << state.joint_position[0] << std::endl;
    std::cout << "TCP X = " << state.tcp_pose[0] << std::endl;
}
```

---

### SetCriDataReceived

```cpp
void SetCriDataReceived(std::function<void(const ClientRealtimeState&)> cb);
```

设置 CRI 数据回调。每次收到 CRI 帧时触发。避免在回调中长时间阻塞。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `cb` | `std::function<void(const ClientRealtimeState&)>` | -- | 回调函数 |

**返回值：** 无

**异常：** 无

```cpp
robot.SetCriDataReceived([](const Codroid::ClientRealtimeState& data) {
    std::cout << "Joints: ";
    for (auto j : data.joint_position) std::cout << j << " ";
    std::cout << std::endl;
});
```

---

### WaitForCriData

```cpp
void WaitForCriData(double timeout_s = 5.0);
```

阻塞等待第一个 CRI 数据帧到达。调用 `*Sync` 阻塞运动方法前必须确保 CRI 数据已开始推送。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `timeout_s` | `double` | 5.0 | 最大等待秒数 |

**返回值：** 无

**异常：** `std::runtime_error` -- 超时未收到数据

```cpp
robot.StartCriDataPush("192.168.8.150", 18888);
robot.WaitForCriData(5.0); // 等待首帧
// 现在可以安全调用 *Sync 方法
```

---

## 工程/脚本

### RunScript

```cpp
CommandResult RunScript(const std::string& mainScript,
                        const std::unordered_map<std::string, std::string>& subThreads = {},
                        const std::unordered_map<std::string, std::string>& subPrograms = {},
                        const std::unordered_map<std::string, std::string>& interrupts = {},
                        const nlohmann::json& vars = {},
                        int id = 1);
```

运行远程 Lua 脚本。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `mainScript` | `const std::string&` | -- | 主脚本内容 |
| `subThreads` | `const std::unordered_map<std::string, std::string>&` | `{}` | 子线程脚本 |
| `subPrograms` | `const std::unordered_map<std::string, std::string>&` | `{}` | 子程序脚本 |
| `interrupts` | `const std::unordered_map<std::string, std::string>&` | `{}` | 中断处理脚本 |
| `vars` | `const nlohmann::json&` | `{}` | 注入变量 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.RunScript("movej(j1, v50) sub1() end");
```

---

### EnterRemoteScriptMode

```cpp
CommandResult EnterRemoteScriptMode(int id = 1);
```

进入远程脚本模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### Run

```cpp
CommandResult Run(const std::string& projectId, int id = 1);
```

按工程 ID 运行工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `projectId` | `const std::string&` | -- | 工程 ID |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### RunByIndex

```cpp
CommandResult RunByIndex(int index, int id = 1);
```

通过索引运行工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `index` | `int` | -- | 工程索引 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### RunStep

```cpp
CommandResult RunStep(const std::string& projectId, int id = 1);
```

单步运行工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `projectId` | `const std::string&` | -- | 工程 ID |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### PauseProject

```cpp
CommandResult PauseProject(int id = 1);
```

暂停工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ResumeProject

```cpp
CommandResult ResumeProject(int id = 1);
```

恢复工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopProject

```cpp
CommandResult StopProject(int id = 1);
```

停止工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## 全局变量

### GetGlobalVars

```cpp
nlohmann::json GetGlobalVars(int id = 1);
```

获取所有全局变量（原始 JSON 响应）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `nlohmann::json` -- 控制器返回的原始 JSON

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
nlohmann::json vars = robot.GetGlobalVars();
std::cout << vars.dump(2) << std::endl;
```

---

### SaveGlobalVars

```cpp
CommandResult SaveGlobalVars(const std::map<std::string, Variable>& vars, int id = 1);
```

保存全局变量。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `vars` | `const std::map<std::string, Variable>&` | -- | 变量名到值的映射 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**Variable 结构：**

```cpp
struct Variable {
    std::string val;  // JSON 字符串
    std::string nm;   // 备注
    template<typename T>
    Variable(const T& value, const std::string& note = "");
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `val` | `std::string` | JSON 格式的值 |
| `nm` | `std::string` | 备注信息 |

```cpp
std::map<std::string, Codroid::Variable> vars = {
    {"counter", Codroid::Variable(42, "计数器")},
    {"name", Codroid::Variable(std::string("test"), "名称")}
};
robot.SaveGlobalVars(vars);
```

---

### RemoveGlobalVars

```cpp
CommandResult RemoveGlobalVars(const std::vector<std::string>& names, int id = 1);
```

删除全局变量。删除不存在的变量不会报错。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `names` | `const std::vector<std::string>&` | -- | 要删除的变量名列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.RemoveGlobalVars({"counter", "name"});
```

---

## 运动学

### ForwardKinematics

```cpp
std::vector<double> ForwardKinematics(const FKParams& params, int id = 1);
```

正运动学：关节角 -> TCP 位姿。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const FKParams&` | -- | 正解参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<double>` -- TCP 位姿 [x,y,z,rx,ry,rz]（mm + 度）

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**FKParams 结构：**

```cpp
struct FKParams {
    std::vector<double> jp;    // 关节角（度）
    std::vector<double> coor;  // 可选用户坐标系
    std::vector<double> tool;  // 可选工具坐标系
    std::vector<double> ep;    // 可选附加轴
    explicit FKParams(const std::vector<double>& jointPos);
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `jp` | `std::vector<double>` | 关节角（度），六轴 |
| `coor` | `std::vector<double>` | 用户坐标系 [x,y,z,a,b,c]（可选） |
| `tool` | `std::vector<double>` | 工具坐标系（可选） |
| `ep` | `std::vector<double>` | 附加轴（可选） |

```cpp
Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
auto pose = robot.ForwardKinematics(fk);
// pose = [x, y, z, rx, ry, rz] in mm + deg
```

---

### InverseKinematics

```cpp
std::vector<double> InverseKinematics(const IKParams& params, int id = 1);
```

逆运动学：TCP 位姿 -> 关节角。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const IKParams&` | -- | 逆解参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<double>` -- 关节角（度），六轴

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**IKParams 结构：**

```cpp
struct IKParams {
    std::vector<double> cp;   // TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）
    std::vector<double> rj;   // 参考关节角（度），用作 IK 起始猜测
    std::vector<double> ep;   // 可选附加轴
    explicit IKParams(const std::vector<double>& cartesianPos);
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `cp` | `std::vector<double>` | TCP 位姿 [x,y,z,rx,ry,rz]（mm + 度） |
| `rj` | `std::vector<double>` | 参考关节角（度），用作 IK 起始猜测 |
| `ep` | `std::vector<double>` | 附加轴（可选） |

```cpp
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
ik.rj = {0, 0, 90, 0, 90, 0}; // 参考关节
auto joints = robot.InverseKinematics(ik);
```

---

### CalculateRelativePose

```cpp
std::vector<double> CalculateRelativePose(const RelativePoseParams& params, int id = 1);
```

笛卡尔相对位姿计算。在用户或工具坐标系中对当前 TCP 施加偏移。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const RelativePoseParams&` | -- | 相对位姿参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<double>` -- 计算后的位姿 [x,y,z,rx,ry,rz]

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**RelativePoseParams 结构：**

```cpp
struct RelativePoseParams {
    std::vector<double> pos;      // 当前位姿 [x,y,z,rx,ry,rz]
    std::vector<double> offset;   // 偏移量 [dx,dy,dz,drx,dry,drz]
    CoorType coorType = CoorType::Tool;  // 坐标系类型
    std::vector<double> posCoor;  // 可选位置坐标系
    std::vector<double> coor;     // 可选坐标系
};
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `pos` | `std::vector<double>` | -- | 当前 TCP 位姿 |
| `offset` | `std::vector<double>` | -- | [dx,dy,dz,drx,dry,drz] 偏移量 |
| `coorType` | `CoorType` | `Tool` | 坐标系类型：`User` 或 `Tool` |
| `posCoor` | `std::vector<double>` | `{}` | 位置坐标系（可选） |
| `coor` | `std::vector<double>` | `{}` | 坐标系定义（可选） |

```cpp
auto state = robot.GetRobotRealtimeState();
Codroid::RelativePoseParams params(
    state.tcp_pose,
    {0, 0, -100, 0, 0, 0},
    Codroid::CoorType::Tool
);
auto newPose = robot.CalculateRelativePose(params);
```

---

## 主题订阅

### SubscribePublishTopic

```cpp
ClientPublishSubscription SubscribePublishTopic(
    std::string topicTy,
    std::function<void(const ClientPublishNotification&)> handler,
    int tc_milliseconds = 100);
```

订阅推送主题。返回订阅句柄，析构时自动停止订阅。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `topicTy` | `std::string` | -- | 主题字符串（如 `"publish/RobotStatus"`） |
| `handler` | `std::function<void(const ClientPublishNotification&)>` | -- | 回调函数 |
| `tc_milliseconds` | `int` | 100 | 推送周期（ms） |

**返回值：** `ClientPublishSubscription` -- 订阅句柄，析构或调用 `Dispose()` 时停止

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientPublishNotification 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `ty` | `std::string` | 主题类型 |
| `db_json` | `std::string` | `db` 子树的 JSON 字符串 |
| `raw_json` | `std::string` | 整帧 JSON |

```cpp
auto sub = robot.SubscribePublishTopic(
    "publish/RobotStatus",
    [](const Codroid::ClientPublishNotification& n) {
        std::cout << "状态更新: " << n.db_json << std::endl;
    }
);
// 订阅在 sub 析构前有效
```

---

## 机器人设置参数

### GetRobotParameters

```cpp
ClientRobotParameters GetRobotParameters(int id = 1);
```

获取设置界面参数（协议 19.7）。返回工具坐标系、载荷坐标系、用户坐标系及默认 ID。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `ClientRobotParameters` -- 机器人完整参数集

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientRobotParameters 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `valid` | `bool` | 数据是否有效（失败时为 false） |
| `default_tool_id` | `int` | 当前激活的工具坐标系编号 |
| `default_payload_id` | `int` | 当前激活的载荷编号 |
| `default_coordinate_id` | `int` | 当前激活的用户坐标系编号 |
| `max_payload` | `double` | 最大载荷质量（kg） |
| `tool` | `std::vector<ClientRobotFrame>` | 工具坐标系列表 |
| `payload` | `std::vector<ClientRobotPayload>` | 载荷坐标系列表 |
| `coordinate` | `std::vector<ClientRobotFrame>` | 用户坐标系列表 |

```cpp
auto params = robot.GetRobotParameters();
if (params.valid) {
    std::cout << "默认工具: " << params.default_tool_id << std::endl;
    std::cout << "最大载荷: " << params.max_payload << " kg" << std::endl;
}
```

---

### SetDefaultPayloadId

```cpp
CommandResult SetDefaultPayloadId(int payloadId, int id = 1);
```

设置默认载荷编号（1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `payloadId` | `int` | -- | 载荷编号（1~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetDefaultToolId

```cpp
CommandResult SetDefaultToolId(int toolId, int id = 1);
```

设置默认工具坐标系编号（1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `toolId` | `int` | -- | 工具编号（1~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetDefaultUserCoordinateId

```cpp
CommandResult SetDefaultUserCoordinateId(int coordinateId, int id = 1);
```

设置默认用户坐标系编号（1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `coordinateId` | `int` | -- | 用户坐标系编号（1~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SaveToolFrames

```cpp
CommandResult SaveToolFrames(const std::vector<ClientRobotFrame>& frames, int id = 1);
```

直接下发完整工具坐标系表（必须包含 id 0~15，id=0 必须全零）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frames` | `const std::vector<ClientRobotFrame>&` | -- | 工具坐标系列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetToolFrame

```cpp
CommandResult SetToolFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);
```

修改单个工具坐标系（先读后改，frame_id 为 1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frame_id` | `int` | -- | 工具坐标系编号（1~15） |
| `frame` | `const ClientRobotFrame&` | -- | 坐标系定义 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientRobotFrame 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `id` | `int` | 坐标系编号 |
| `x` | `double` | X 偏移（mm） |
| `y` | `double` | Y 偏移（mm） |
| `z` | `double` | Z 偏移（mm） |
| `a` | `double` | 绕 X 旋转（度） |
| `b` | `double` | 绕 Y 旋转（度） |
| `c` | `double` | 绕 Z 旋转（度） |

```cpp
Codroid::CodroidClient::ClientRobotFrame frame;
frame.id = 1; frame.x = 0; frame.y = 0; frame.z = 100;
frame.a = 0; frame.b = 0; frame.c = 0;
robot.SetToolFrame(1, frame);
```

---

### SavePayloadFrames

```cpp
CommandResult SavePayloadFrames(const std::vector<ClientRobotPayload>& frames, int id = 1);
```

直接下发完整载荷坐标系表。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frames` | `const std::vector<ClientRobotPayload>&` | -- | 载荷坐标系列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetPayloadFrame

```cpp
CommandResult SetPayloadFrame(int frame_id, const ClientRobotPayload& frame, int id = 1);
```

修改单个载荷坐标系（先读后改，frame_id 为 1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frame_id` | `int` | -- | 载荷编号（1~15） |
| `frame` | `const ClientRobotPayload&` | -- | 载荷定义 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientRobotPayload 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `id` | `int` | 载荷编号 |
| `m` | `double` | 质量（kg） |
| `mx` | `double` | 质心 X 偏移（mm） |
| `my` | `double` | 质心 Y 偏移（mm） |
| `mz` | `double` | 质心 Z 偏移（mm） |

```cpp
Codroid::CodroidClient::ClientRobotPayload payload;
payload.id = 1; payload.m = 2.5;
payload.mx = 0; payload.my = 0; payload.mz = 50;
robot.SetPayloadFrame(1, payload);
```

---

### SetUserCoordinateFrame

```cpp
CommandResult SetUserCoordinateFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);
```

修改单个用户坐标系（先读后改，frame_id 为 1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frame_id` | `int` | -- | 用户坐标系编号（1~15） |
| `frame` | `const ClientRobotFrame&` | -- | 坐标系定义 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
Codroid::CodroidClient::ClientRobotFrame frame;
frame.id = 1; frame.x = 100; frame.y = 200; frame.z = 0;
frame.a = 0; frame.b = 0; frame.c = 45;
robot.SetUserCoordinateFrame(1, frame);
```

---

### SetPayload

```cpp
CommandResult SetPayload(int payloadId, int id = 1);
```

运行时切换当前载荷。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `payloadId` | `int` | -- | 载荷槽位编号（0~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetManualMoveRate

```cpp
CommandResult SetManualMoveRate(int pct, int id = 1);
```

设置手动运动倍率（0~100）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `pct` | `int` | -- | 运动倍率百分比（0~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetManualMoveRate(50);  // 50% 速度
```

---

### SetAutoMoveRate

```cpp
CommandResult SetAutoMoveRate(int pct, int id = 1);
```

设置自动运动倍率（0~100）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `pct` | `int` | -- | 运动倍率百分比（0~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetAutoMoveRate(100);  // 全速
```

---

### SetCollisionSensitivity

```cpp
CommandResult SetCollisionSensitivity(int sensitivity, int id = 1);
```

设置碰撞检测灵敏度（0~100）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `sensitivity` | `int` | -- | 碰撞灵敏度（0~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetCollisionSensitivity(50);
```

---

## 错误处理设置

### SetThrowOnCommandError

```cpp
void SetThrowOnCommandError(bool enable);
```

设置为 `true` 时，指令失败抛 `CodroidCommandException`；默认为 `false`，通过 `CommandResult::error_msg` 返回。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enable` | `bool` | -- | 是否启用异常模式 |

**返回值：** 无

**异常：** 无

```cpp
robot.SetThrowOnCommandError(true);
try {
    robot.SetDo(999, 1); // 无效端口，将抛异常
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << "控制器错误: " << ex.controller_error() << std::endl;
}
```

---

### ThrowOnCommandError

```cpp
bool ThrowOnCommandError() const noexcept;
```

获取当前错误处理模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `bool` -- `true` 表示异常模式，`false` 表示返回值模式

**异常：** 无

---

## 完整示例

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    try {
        // 1. 连接、切换远程、上电
        if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
            std::cerr << "连接失败" << std::endl;
            return 1;
        }

        // 2. 等待 CRI 数据（阻塞运动必需）
        robot.WaitForCriData(5.0);

        // 3. IO 操作
        int di0 = robot.GetDi(0);
        std::cout << "DI 0 = " << di0 << std::endl;
        robot.SetDo(10, di0);

        // 4. 寄存器
        double reg = robot.GetRegisterValue(49100);
        std::cout << "Register 49100 = " << reg << std::endl;
        robot.SetRegisterValue(49100, reg + 1);

        // 5. 非阻塞运动
        robot.MovJ(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100);

        // 6. 阻塞运动
        auto state = robot.GetRobotRealtimeState();
        robot.MovLSync(
            Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
            150, 500);

        // 7. 多段路径
        std::vector<Codroid::ClientMoveInstruction> path = {
            Codroid::ClientMoveInstruction::MovJ(
                Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100),
            Codroid::ClientMoveInstruction::MovL(
                Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}), 150, 500)
        };
        robot.Move(path);

        // 8. 全局变量
        std::map<std::string, Codroid::Variable> vars = {
            {"counter", Codroid::Variable(42, "计数器")}
        };
        robot.SaveGlobalVars(vars);

        // 9. 运动学
        Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
        auto pose = robot.ForwardKinematics(fk);

    } catch (const Codroid::CodroidCommandException& ex) {
        std::cerr << "指令失败: " << ex.controller_error() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "错误: " << ex.what() << std::endl;
    }

    robot.Disconnect();
    return 0;
}
```
