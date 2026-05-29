# 辅助工具 API

本章介绍主题订阅、全局变量、运动学和控制台 UTF-8 等辅助功能。

---

## 主题订阅

### SubscribePublishTopic

```cpp
ClientPublishSubscription SubscribePublishTopic(
    std::string topicTy,
    std::function<void(const ClientPublishNotification&)> handler,
    int tc_milliseconds = 100);
```

订阅控制器推送的主题。

**参数：**
- `topicTy` — 主题字符串（如 `"publish/RobotStatus"`）
- `handler` — 回调函数
- `tc_milliseconds` — 推送周期，默认 100 ms

**返回：** 订阅句柄，析构时自动停止订阅

---

### 可用主题

| 主题 | 说明 | 数据格式 |
|------|------|----------|
| `publish/ProjectState` | 工程运行状态 | ProjectState |
| `publish/VarUpdate` | 变量更新 | JSON |
| `publish/RobotStatus` | 机器人状态 | RobotStatus |
| `publish/RobotPosture` | 机器人位姿 | RobotPosture |
| `publish/RobotCoordinate` | 坐标系 | JSON |
| `publish/Log` | 日志 | JSON |
| `publish/Error` | 错误 | JSON |

---

### PublishTopics 常量

```cpp
struct PublishTopics {
    static constexpr const char* ProjectState = "publish/ProjectState";
    static constexpr const char* VarUpdate = "publish/VarUpdate";
    static constexpr const char* RobotStatus = "publish/RobotStatus";
    static constexpr const char* RobotPosture = "publish/RobotPosture";
    static constexpr const char* RobotCoordinate = "publish/RobotCoordinate";
    static constexpr const char* Log = "publish/Log";
    static constexpr const char* Error = "publish/Error";
};
```

**使用示例：**
```cpp
// 使用常量
auto sub = robot.SubscribePublishTopic(
    Codroid::PublishTopics::RobotStatus,
    [](const Codroid::ClientPublishNotification& n) {
        std::cout << "状态更新: " << n.db_json << std::endl;
    }
);

// 使用字符串
auto sub2 = robot.SubscribePublishTopic(
    "publish/ProjectState",
    [](const Codroid::ClientPublishNotification& n) {
        std::cout << "工程状态: " << n.db_json << std::endl;
    }
);
```

---

### ClientPublishNotification

```cpp
struct ClientPublishNotification {
    std::string ty;        // 主题类型
    std::string db_json;   // db 子树的 JSON 字符串
    std::string raw_json;  // 整帧 JSON
};
```

**使用示例：**
```cpp
auto sub = robot.SubscribePublishTopic(
    "publish/RobotStatus",
    [](const Codroid::ClientPublishNotification& n) {
        // 解析 JSON
        auto db = nlohmann::json::parse(n.db_json);

        // 读取字段
        if (db.contains("mode")) {
            int mode = db["mode"];
            std::cout << "模式: " << mode << std::endl;
        }

        if (db.contains("state")) {
            int state = db["state"];
            std::cout << "状态: " << state << std::endl;
        }
    }
);
```

---

### ClientPublishSubscription

订阅句柄。析构或 `Dispose()` 后停止本地分发。

```cpp
class ClientPublishSubscription {
public:
    ClientPublishSubscription();
    ~ClientPublishSubscription();

    // 不可复制
    ClientPublishSubscription(const ClientPublishSubscription&) = delete;
    ClientPublishSubscription& operator=(const ClientPublishSubscription&) = delete;

    // 可移动
    ClientPublishSubscription(ClientPublishSubscription&&) noexcept;
    ClientPublishSubscription& operator=(ClientPublishSubscription&&) noexcept;

    void Dispose();           // 提前结束订阅
    bool IsValid() const noexcept;  // 是否仍绑定有效内部资源
};
```

**生命周期管理：**
```cpp
{
    auto sub = robot.SubscribePublishTopic(
        "publish/RobotStatus",
        [](const Codroid::ClientPublishNotification& n) {
            std::cout << n.db_json << std::endl;
        }
    );

    // 订阅在此作用域内有效
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 可选：提前停止
    sub.Dispose();
}
// sub 析构，订阅自动停止
```

---

## 全局变量

### GetGlobalVars

```cpp
nlohmann::json GetGlobalVars(int id = 1);
```

获取所有全局变量（原始 JSON 响应）。

**返回：** JSON 对象

**使用示例：**
```cpp
auto vars = robot.GetGlobalVars();
std::cout << "全局变量: " << vars.dump(2) << std::endl;

// 遍历变量
for (auto& [name, value] : vars.items()) {
    std::cout << name << " = " << value << std::endl;
}
```

---

### SaveGlobalVars

```cpp
CommandResult SaveGlobalVars(const std::map<std::string, Variable>& vars, int id = 1);
```

保存全局变量。

**参数：**
- `vars` — 变量映射（名称 → Variable）

**Variable：**
```cpp
struct Variable {
    std::string val;  // JSON 字符串
    std::string nm;   // 备注

    template<typename T>
    Variable(const T& value, const std::string& note = "");
    Variable() = default;
};
```

**使用示例：**
```cpp
std::map<std::string, Codroid::Variable> vars = {
    {"counter", Codroid::Variable(42, "计数器")},
    {"name", Codroid::Variable(std::string("test"), "名称")},
    {"flag", Codroid::Variable(true, "标志")}
};

auto result = robot.SaveGlobalVars(vars);
if (!result.Ok()) {
    std::cerr << "保存失败: " << result.error_msg << std::endl;
}
```

---

### RemoveGlobalVars

```cpp
CommandResult RemoveGlobalVars(const std::vector<std::string>& names, int id = 1);
```

删除全局变量。

**参数：**
- `names` — 变量名列表

**使用示例：**
```cpp
std::vector<std::string> names = {"counter", "name"};
robot.RemoveGlobalVars(names);
```

---

## 运动学

### ForwardKinematics

```cpp
std::vector<double> ForwardKinematics(const FKParams& params, int id = 1);
```

正解：关节角 → TCP 位姿。

**FKParams：**
```cpp
struct FKParams {
    std::vector<double> jp;    // 关节角（度）
    std::vector<double> coor;  // 可选用户坐标系
    std::vector<double> tool;  // 可选工具坐标系
    std::vector<double> ep;    // 可选附加轴

    explicit FKParams(const std::vector<double>& jointPos);
    FKParams() = default;
};
```

**返回：** TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）

**使用示例：**
```cpp
// 基本用法
Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
auto tcp = robot.ForwardKinematics(fk);

std::cout << "TCP 位姿: ";
for (auto v : tcp) std::cout << v << " ";
std::cout << std::endl;

// 使用当前关节位置
auto state = robot.GetRobotRealtimeState();
Codroid::FKParams fk2(state.joint_position);
auto tcp2 = robot.ForwardKinematics(fk2);
```

---

### InverseKinematics

```cpp
std::vector<double> InverseKinematics(const IKParams& params, int id = 1);
```

逆解：TCP 位姿 → 关节角。

**IKParams：**
```cpp
struct IKParams {
    std::vector<double> cp;   // TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）
    std::vector<double> rj;   // 参考关节角（度）
    std::vector<double> ep;   // 可选附加轴

    explicit IKParams(const std::vector<double>& cartesianPos);
    IKParams() = default;
};
```

**返回：** 关节角（度）

**使用示例：**
```cpp
// 基本用法
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
auto joints = robot.InverseKinematics(ik);

std::cout << "关节角: ";
for (auto v : joints) std::cout << v << " ";
std::cout << std::endl;

// 指定参考关节（避免跳解）
Codroid::IKParams ik2({400, 0, 300, 180, 0, 0});
ik2.rj = {0, 0, 90, 0, 90, 0};
auto joints2 = robot.InverseKinematics(ik2);
```

---

### CalculateRelativePose

```cpp
std::vector<double> CalculateRelativePose(const RelativePoseParams& params, int id = 1);
```

笛卡尔相对位姿计算。

**RelativePoseParams：**
```cpp
struct RelativePoseParams {
    std::vector<double> pos;      // 当前位姿
    std::vector<double> offset;   // 偏移量
    CoorType coorType = CoorType::Tool;
    std::vector<double> posCoor;  // 可选位置坐标系
    std::vector<double> coor;     // 可选坐标系

    RelativePoseParams(const std::vector<double>& p, const std::vector<double>& o, CoorType type);
    RelativePoseParams() = default;
};
```

**使用示例：**
```cpp
// 在工具系下偏移
std::vector<double> current = {400, 0, 300, 180, 0, 0};
std::vector<double> offset = {100, 0, 0, 0, 0, 0};  // X 方向偏移 100mm

Codroid::RelativePoseParams params(current, offset, Codroid::CoorType::Tool);
auto newPose = robot.CalculateRelativePose(params);

std::cout << "新位姿: ";
for (auto v : newPose) std::cout << v << " ";
std::cout << std::endl;
```

---

## 控制台 UTF-8

### ConsoleUtf8

Windows 控制台 UTF-8 支持。

```cpp
#include "Codroid/console_utf8.hpp"
```

#### InitConsoleUtf8

```cpp
static void InitConsoleUtf8();
```

初始化 Windows 控制台 UTF-8 编码。

**使用场景：** 在 Windows 上输出中文时调用。

**使用示例：**
```cpp
#include "Codroid/client.hpp"
#include "Codroid/console_utf8.hpp"
#include <iostream>

int main() {
    // Windows 控制台 UTF-8 支持
    #ifdef _WIN32
    Codroid::ConsoleUtf8::InitConsoleUtf8();
    #endif

    std::cout << "中文输出测试" << std::endl;

    Codroid::CodroidClient robot;
    // ...

    return 0;
}
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

**参数：**
- `mainScript` — 主脚本代码
- `subThreads` — 子线程（名称 → 代码）
- `subPrograms` — 子程序（名称 → 代码）
- `interrupts` — 中断（名称 → 代码）
- `vars` — 变量

**使用示例：**
```cpp
std::string mainScript = R"(
    print("Hello from Lua")
    local counter = 0
    for i = 1, 10 do
        counter = counter + i
    end
    print("Sum: " .. counter)
)";

auto result = robot.RunScript(mainScript);
if (!result.Ok()) {
    std::cerr << "脚本执行失败: " << result.error_msg << std::endl;
}
```

---

### EnterRemoteScriptMode

```cpp
CommandResult EnterRemoteScriptMode(int id = 1);
```

进入远程脚本模式。

---

### Run / RunByIndex / RunStep

```cpp
CommandResult Run(const std::string& projectId, int id = 1);
CommandResult RunByIndex(int index, int id = 1);
CommandResult RunStep(const std::string& projectId, int id = 1);
```

运行工程。

**参数：**
- `projectId` — 工程 ID
- `index` — 工程索引

**使用示例：**
```cpp
// 通过 ID 运行
robot.Run("project_001");

// 通过索引运行
robot.RunByIndex(0);

// 单步运行
robot.RunStep("project_001");
```

---

### PauseProject / ResumeProject / StopProject

```cpp
CommandResult PauseProject(int id = 1);
CommandResult ResumeProject(int id = 1);
CommandResult StopProject(int id = 1);
```

暂停、恢复、停止工程。

**使用示例：**
```cpp
// 运行工程
robot.Run("project_001");

// 暂停
robot.PauseProject();

// 恢复
robot.ResumeProject();

// 停止
robot.StopProject();
```

---

## 完整示例：主题订阅

```cpp
#include "Codroid/client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 订阅机器人状态
    auto statusSub = robot.SubscribePublishTopic(
        Codroid::PublishTopics::RobotStatus,
        [](const Codroid::ClientPublishNotification& n) {
            auto db = nlohmann::json::parse(n.db_json);
            std::cout << "[状态] 模式=" << db["mode"]
                      << " 状态=" << db["state"]
                      << " 运动中=" << db["isMoving"]
                      << std::endl;
        },
        50  // 50ms 周期
    );

    // 订阅工程状态
    auto projectSub = robot.SubscribePublishTopic(
        Codroid::PublishTopics::ProjectState,
        [](const Codroid::ClientPublishNotification& n) {
            auto db = nlohmann::json::parse(n.db_json);
            std::cout << "[工程] ID=" << db["id"]
                      << " 状态=" << db["state"]
                      << std::endl;
        }
    );

    // 订阅错误
    auto errorSub = robot.SubscribePublishTopic(
        Codroid::PublishTopics::Error,
        [](const Codroid::ClientPublishNotification& n) {
            std::cerr << "[错误] " << n.db_json << std::endl;
        }
    );

    // 等待一段时间接收推送
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 订阅在作用域结束时自动停止
    robot.Disconnect();
    return 0;
}
```

---

## 完整示例：运动学计算

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 正解：关节角 → TCP
    Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
    auto tcp = robot.ForwardKinematics(fk);

    std::cout << "正解结果:" << std::endl;
    std::cout << "  X=" << tcp[0] << " mm" << std::endl;
    std::cout << "  Y=" << tcp[1] << " mm" << std::endl;
    std::cout << "  Z=" << tcp[2] << " mm" << std::endl;
    std::cout << "  Rx=" << tcp[3] << " deg" << std::endl;
    std::cout << "  Ry=" << tcp[4] << " deg" << std::endl;
    std::cout << "  Rz=" << tcp[5] << " deg" << std::endl;

    // 逆解：TCP → 关节角
    Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
    ik.rj = {0, 0, 90, 0, 90, 0};  // 参考关节
    auto joints = robot.InverseKinematics(ik);

    std::cout << "逆解结果:" << std::endl;
    for (int i = 0; i < 6; i++) {
        std::cout << "  J" << (i+1) << "=" << joints[i] << " deg" << std::endl;
    }

    // 相对位姿计算
    std::vector<double> current = {400, 0, 300, 180, 0, 0};
    std::vector<double> offset = {50, 0, 0, 0, 0, 0};  // X 方向偏移 50mm

    Codroid::RelativePoseParams relParams(current, offset, Codroid::CoorType::Tool);
    auto newPose = robot.CalculateRelativePose(relParams);

    std::cout << "相对位姿计算:" << std::endl;
    std::cout << "  原始: " << current[0] << ", " << current[1] << ", " << current[2] << std::endl;
    std::cout << "  新位: " << newPose[0] << ", " << newPose[1] << ", " << newPose[2] << std::endl;

    robot.Disconnect();
    return 0;
}
```

---

## 最佳实践

### 1. 及时停止订阅

```cpp
{
    auto sub = robot.SubscribePublishTopic(...);
    // 使用订阅
    // ...
    sub.Dispose();  // 提前停止
}
// 或让析构函数自动停止
```

### 2. 使用 PublishTopics 常量

```cpp
// 好：使用常量
robot.SubscribePublishTopic(Codroid::PublishTopics::RobotStatus, ...);

// 不推荐：硬编码字符串
robot.SubscribePublishTopic("publish/RobotStatus", ...);
```

### 3. 运动学参考关节

```cpp
// 好：指定参考关节
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
ik.rj = currentState.joint_position;
auto joints = robot.InverseKinematics(ik);

// 不推荐：不指定参考关节
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
auto joints = robot.InverseKinematics(ik);
```

### 4. Windows 控制台 UTF-8

```cpp
// 在 main 函数开头调用
#ifdef _WIN32
Codroid::ConsoleUtf8::InitConsoleUtf8();
#endif
```

---

## 常见问题

### Q1: 主题订阅没有收到数据

1. 确认已连接控制器
2. 检查主题名称是否正确
3. 确认控制器支持该主题

### Q2: 全局变量保存失败

1. 检查变量名是否合法
2. 检查值格式是否正确（JSON 字符串）
3. 确认控制器处于远程模式

### Q3: 运动学计算结果异常

1. 检查关节角范围
2. 检查 TCP 位姿是否在工作空间内
3. 指定参考关节避免多解

### Q4: Windows 控制台中文乱码

1. 确保调用 `ConsoleUtf8::InitConsoleUtf8()`
2. 确保源文件使用 UTF-8 编码
3. 确保控制台字体支持中文
