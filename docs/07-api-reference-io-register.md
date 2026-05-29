# IO 与寄存器 API

本章介绍数字量/模拟量 IO 操作和寄存器读写。

---

## IO 操作

### 数字量输入 (DI)

#### GetDi

```cpp
int GetDi(int port, int id = 1);
```

读取数字量输入。

**参数：**
- `port` — 端口号

**返回：** DI 值（0 或 1）

**示例：**
```cpp
int di0 = robot.GetDi(0);
int di1 = robot.GetDi(1);
std::cout << "DI 0 = " << di0 << ", DI 1 = " << di1 << std::endl;
```

---

### 数字量输出 (DO)

#### GetDo

```cpp
int GetDo(int port, int id = 1);
```

读取数字量输出。

**参数：**
- `port` — 端口号

**返回：** DO 值（0 或 1）

---

#### SetDo

```cpp
CommandResult SetDo(int port, int value, int id = 1);
```

设置数字量输出。

**参数：**
- `port` — 端口号
- `value` — 值（0 或 1）

**示例：**
```cpp
// 设置 DO 10 为 1
auto result = robot.SetDo(10, 1);
if (!result.Ok()) {
    std::cerr << "设置失败: " << result.error_msg << std::endl;
}

// 读取 DI 0 并写入 DO 10
int di0 = robot.GetDi(0);
robot.SetDo(10, di0);
```

---

### 模拟量输入 (AI)

#### GetAi

```cpp
double GetAi(int port, int id = 1);
```

读取模拟量输入。

**参数：**
- `port` — 端口号

**返回：** AI 值

**示例：**
```cpp
double ai0 = robot.GetAi(0);
std::cout << "AI 0 = " << ai0 << std::endl;
```

---

### 模拟量输出 (AO)

#### GetAo

```cpp
double GetAo(int port, int id = 1);
```

读取模拟量输出。

**参数：**
- `port` — 端口号

**返回：** AO 值

---

#### SetAo

```cpp
CommandResult SetAo(int port, double value, int id = 1);
```

设置模拟量输出。

**参数：**
- `port` — 端口号
- `value` — 值

**示例：**
```cpp
// 设置 AO 0 为 3.3V
robot.SetAo(0, 3.3);
```

---

## 寄存器操作

### GetRegisterValue

```cpp
double GetRegisterValue(int address, int id = 1);
```

读取单个寄存器值。

**参数：**
- `address` — 寄存器地址

**返回：** 寄存器值

**示例：**
```cpp
double value = robot.GetRegisterValue(49100);
std::cout << "寄存器 49100 = " << value << std::endl;
```

---

### GetRegisterValues

```cpp
std::vector<ClientRegisterInfo> GetRegisterValues(const std::vector<int>& addresses, int id = 1);
```

批量读取寄存器。

**参数：**
- `addresses` — 地址列表

**返回：** `ClientRegisterInfo` 列表

**ClientRegisterInfo：**
```cpp
struct ClientRegisterInfo {
    int address = 0;    // 地址
    double value = 0.0; // 值
};
```

**示例：**
```cpp
std::vector<int> addresses = {49100, 49101, 49102};
auto values = robot.GetRegisterValues(addresses);

for (const auto& reg : values) {
    std::cout << "寄存器 " << reg.address << " = " << reg.value << std::endl;
}
```

---

### SetRegisterValue

```cpp
CommandResult SetRegisterValue(int address, double value, int id = 1);
```

设置寄存器值。

**参数：**
- `address` — 寄存器地址
- `value` — 值

**示例：**
```cpp
// 读取并修改
double value = robot.GetRegisterValue(49100);
robot.SetRegisterValue(49100, value + 1);
```

---

## ClientIoInfo / IOInfo

IO 读回信息结构体。

```cpp
struct ClientIoInfo {
    std::string type;   // 类型字符串（DI/DO/AI/AO）
    int port = 0;       // 端口号
    double value = 0.0; // 值
};
```

---

## 完整示例

### 基础 IO 操作

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 读取 DI
    for (int i = 0; i < 8; i++) {
        int di = robot.GetDi(i);
        std::cout << "DI " << i << " = " << di << std::endl;
    }

    // 读取 AI
    for (int i = 0; i < 4; i++) {
        double ai = robot.GetAi(i);
        std::cout << "AI " << i << " = " << ai << std::endl;
    }

    // 设置 DO
    robot.SetDo(0, 1);
    robot.SetDo(1, 0);

    // 设置 AO
    robot.SetAo(0, 3.3);

    // 读取 DO 状态
    int do0 = robot.GetDo(0);
    std::cout << "DO 0 = " << do0 << std::endl;

    robot.Disconnect();
    return 0;
}
```

---

### 寄存器读写

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 读取单个寄存器
    double value = robot.GetRegisterValue(49100);
    std::cout << "寄存器 49100 = " << value << std::endl;

    // 修改寄存器
    robot.SetRegisterValue(49100, value + 1);

    // 批量读取
    std::vector<int> addresses = {49100, 49101, 49102, 49103};
    auto values = robot.GetRegisterValues(addresses);

    std::cout << "批量读取:" << std::endl;
    for (const auto& reg : values) {
        std::cout << "  " << reg.address << " = " << reg.value << std::endl;
    }

    // 批量修改
    for (const auto& reg : values) {
        robot.SetRegisterValue(reg.address, reg.value * 2);
    }

    robot.Disconnect();
    return 0;
}
```

---

### DI 触发运动

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

    // 启动 CRI 推送（用于阻塞运动）
    robot.StartCriDataPush("192.168.8.150", 9030);
    robot.WaitForCriData(5.0);

    std::cout << "等待 DI 0 触发..." << std::endl;

    // 等待 DI 0 为 1
    while (robot.GetDi(0) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "DI 0 触发，开始运动" << std::endl;

    // 执行运动
    auto target = Codroid::CartesianPoint::MmDegWithRef(
        {400, 0, 300, 180, 0, 0},
        robot.GetRobotRealtimeState().joint_position
    );
    robot.MovLSync(target, 150, 500);

    std::cout << "运动完成" << std::endl;

    // 设置 DO 0 指示完成
    robot.SetDo(0, 1);

    robot.StopCriDataPush();
    robot.Disconnect();
    return 0;
}
```

---

## 最佳实践

### 1. 检查返回值

```cpp
// 好：检查返回值
auto result = robot.SetDo(10, 1);
if (!result.Ok()) {
    std::cerr << "设置失败: " << result.error_msg << std::endl;
    // 错误处理
}

// 不推荐：忽略返回值
robot.SetDo(10, 1);
```

### 2. 使用异常模式

```cpp
robot.SetThrowOnCommandError(true);

try {
    robot.SetDo(10, 1);
    robot.SetRegisterValue(49100, 100);
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << "错误: " << ex.controller_error() << std::endl;
}
```

### 3. 批量读取提高效率

```cpp
// 好：批量读取
std::vector<int> addresses = {49100, 49101, 49102};
auto values = robot.GetRegisterValues(addresses);

// 不推荐：逐个读取
double v1 = robot.GetRegisterValue(49100);
double v2 = robot.GetRegisterValue(49101);
double v3 = robot.GetRegisterValue(49102);
```

### 4. IO 端口范围

- DI/DO 端口范围取决于控制器硬件配置
- AI/AO 端口范围取决于控制器硬件配置
- 使用无效端口会返回错误

---

## 常见问题

### Q1: GetDi 返回值始终为 0

1. 检查端口号是否正确
2. 检查硬件连接
3. 确认控制器已上电

### Q2: SetDo 不生效

1. 检查端口号是否正确
2. 检查返回值是否有错误
3. 确认控制器处于远程模式

### Q3: 寄存器地址范围

寄存器地址范围取决于控制器固件版本。常见地址：
- 49100~49199：通用寄存器
- 其他地址请参考控制器文档

### Q4: 模拟量精度

AI/AO 精度取决于硬件配置。SDK 返回 `double` 类型，实际精度由硬件决定。
