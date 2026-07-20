# 运动控制 API

本章介绍运动相关的类型和 API：点位表示、路径指令、阻塞运动。

---

## 点位类型

### JointPoint

关节空间目标点（六轴角，单位：度）。

```cpp
struct JointPoint {
    std::vector<double> jp;  // 六轴关节角（度）

    static JointPoint Degrees(std::vector<double> joints_deg);
};
```

**工厂方法：**
```cpp
// 六轴关节角（度）
auto joints = Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0});
```

**使用场景：**
- `MovJ(JointPoint)` — 关节运动到关节目标
- `MovL(JointPoint)` — 直线运动到关节目标
- `MoveInstruction::MovJ(JointPoint)` — 路径段
- `MoveToTarget::Joint(JointPoint)` — MoveTo 规划

---

### CartesianPoint

笛卡尔末端目标点（TCP 位姿：mm + 度）。

```cpp
struct CartesianPoint {
    std::vector<double> cp;  // [x, y, z, rx, ry, rz]，前三 mm，后三度
    std::vector<double> rj;  // 逆解参考关节角（度）

    static CartesianPoint MmDeg(std::vector<double> pose_mm_deg);
    static CartesianPoint MmDegWithRef(std::vector<double> pose_mm_deg, std::vector<double> ref_joints_deg);
};
```

**工厂方法：**

#### MmDeg

仅 TCP 位姿，不指定参考关节。

```cpp
auto pose = Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0});
```

#### MmDegWithRef

TCP 位姿 + 逆解参考关节。**建议**用 CRI `joint_position` 当前值作 `rj`。

```cpp
auto state = robot.GetRobotRealtimeState();
auto pose = Codroid::CartesianPoint::MmDegWithRef(
    {400, 0, 300, 180, 0, 0},
    state.joint_position
);
```

**为什么需要 `rj`？**
- 多组关节解时控制器据此选解，避免跳解
- 建议始终使用 `MmDegWithRef`

**使用场景：**
- `MovL(CartesianPoint)` — 直线运动到笛卡尔目标
- `MovJ(CartesianPoint)` — 关节运动到笛卡尔目标（控制器做逆解）
- `MoveInstruction::MovL(CartesianPoint)` — 路径段
- `MoveToTarget::Cartesian(CartesianPoint)` — MoveTo 规划

---

### MovePoint

路径段中的单个目标点（协议层）。

```cpp
struct MovePoint {
    std::vector<double> jp;  // 关节角（度），与 cp 二选一
    std::vector<double> cp;  // TCP [x,y,z,rx,ry,rz]（mm+度）
    std::vector<double> rj;  // 仅 cp 有效时：逆解参考关节（度）
    std::vector<double> ep;  // 附加轴（可选）

    static MovePoint Joint(JointPoint joint);
    static MovePoint Cartesian(CartesianPoint cart);
};
```

**工厂方法：**
```cpp
// 从 JointPoint 生成
auto mp1 = Codroid::MovePoint::Joint(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}));

// 从 CartesianPoint 生成
auto mp2 = Codroid::MovePoint::Cartesian(Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}));
```

**注意：** 每条路径点只应填 **jp 或 cp 之一**（打包时 jp 优先）。

---

## 路径指令

### ClientMoveInstruction / MoveInstruction

路径中的一段运动指令。

```cpp
struct ClientMoveInstruction {
    ClientMoveType type = ClientMoveType::MovJ;
    double speed = 60.0;
    double acceleration = 150.0;
    double blend = -1.0;
    double relative_blend = -1.0;
    int circle_num = 1;
    ClientMovePoint target;
    ClientMovePoint middle;
    std::vector<double> coor;
    std::vector<double> tool;

    // 静态工厂方法
    static ClientMoveInstruction MovJ(ClientJointPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovJ(ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovL(ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovL(ClientJointPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovC(ClientCartesianPoint middle, ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovCircle(ClientCartesianPoint middle, ClientCartesianPoint target, int circle_num, double speed, double acceleration, double blend = -1.0);
};
```

### ClientMoveType / MoveType

```cpp
enum class ClientMoveType {
    MovJ,       // 关节运动
    MovL,       // 直线（笛卡尔）
    MovC,       // 圆弧（经由中间点）
    MovCircle   // 整圆/多圈
};
```

### 运动类型详解

#### MovJ — 关节运动

关节空间插补，目标可为关节角或笛卡尔位姿。

```cpp
// 目标为关节角
auto inst1 = Codroid::ClientMoveInstruction::MovJ(
    Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}),
    40, 100
);

// 目标为笛卡尔位姿（控制器做逆解）
auto inst2 = Codroid::ClientMoveInstruction::MovJ(
    Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}),
    40, 100
);
```

#### MovL — 直线运动

笛卡尔直线插补，目标可为笛卡尔位姿或关节角。

```cpp
// 目标为笛卡尔位姿
auto inst = Codroid::ClientMoveInstruction::MovL(
    Codroid::CartesianPoint::MmDegWithRef(
        {400, 0, 300, 180, 0, 0},
        state.joint_position
    ),
    150, 500
);
```

#### MovC — 圆弧运动

经由中间点的圆弧。

```cpp
auto inst = Codroid::ClientMoveInstruction::MovC(
    Codroid::CartesianPoint::MmDeg({450, 50, 300, 180, 0, 0}),  // 中间点
    Codroid::CartesianPoint::MmDeg({500, 0, 300, 180, 0, 0}),   // 目标点
    100, 300
);
```

#### MovCircle — 整圆运动

整圆/多圈运动。

```cpp
auto inst = Codroid::ClientMoveInstruction::MovCircle(
    Codroid::CartesianPoint::MmDeg({450, 50, 300, 180, 0, 0}),  // 中间点
    Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}),   // 目标点
    2,    // 圈数
    100, 300
);
```

### blend（过渡半径）

- `blend < 0`：不下发，用控制器默认（精确停止）
- `blend = 0`：精确停止
- `blend > 0`：过渡半径（mm），路径在目标点附近平滑过渡

```cpp
// 精确停止
auto inst1 = Codroid::ClientMoveInstruction::MovJ(joints, 40, 100, 0);

// 过渡半径 50mm
auto inst2 = Codroid::ClientMoveInstruction::MovJ(joints, 40, 100, 50);
```

---

## 路径执行

### Move / MovePath

```cpp
CommandResult Move(const std::vector<ClientMoveInstruction>& path, int id = 1);
CommandResult MovePath(const std::vector<ClientMoveInstruction>& path, int id = 1);
```

多段路径按顺序执行。

**示例：**
```cpp
auto state = robot.GetRobotRealtimeState();

std::vector<Codroid::ClientMoveInstruction> path = {
    // 段 1：关节运动到起始位
    Codroid::ClientMoveInstruction::MovJ(
        Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}),
        40, 100, 0
    ),
    // 段 2：直线运动到目标位
    Codroid::ClientMoveInstruction::MovL(
        Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
        150, 500, 0
    ),
    // 段 3：圆弧运动
    Codroid::ClientMoveInstruction::MovC(
        Codroid::CartesianPoint::MmDeg({450, 50, 300, 180, 0, 0}),
        Codroid::CartesianPoint::MmDeg({500, 0, 300, 180, 0, 0}),
        100, 300
    )
};

robot.Move(path);
```

---

## 阻塞运动（Sync）

阻塞式运动，等待 CRI 确认到达目标后才返回。

### 前置条件

调用 `*Sync` 方法前，必须确保 CRI 数据已开始推送：

```cpp
// 1. 连接
robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150");

// 2. 启动 CRI 推送
robot.StartCriDataPush("192.168.1.150", 9030);

// 3. 等待首帧
robot.WaitForCriData(5.0);

// 4. 现在可以使用 *Sync 方法
robot.MovLSync(target, 150, 500);
```

### MoveSync

```cpp
bool MoveSync(const std::vector<ClientMoveInstruction>& path, const MotionWaitOptions& wait = {});
```

阻塞式路径执行。

### MovJSync

```cpp
bool MovJSync(const ClientJointPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovJSync(const ClientCartesianPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式关节运动。

### MovLSync

```cpp
bool MovLSync(const ClientCartesianPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovLSync(const ClientJointPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式直线运动。

### MovCSync

```cpp
bool MovCSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
              double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式圆弧运动。

### MovCircleSync

```cpp
bool MovCircleSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   int circle_num, double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式整圆运动。

### MotionWaitOptions

```cpp
struct MotionWaitOptions {
    double timeout_s = 60.0;                        // 整体等待超时（秒）
    double poll_interval_s = 0.05;                  // CRI 轮询间隔（秒）
    double cri_stale_timeout_s = 0.5;               // CRI 数据过期判定（秒）
    int settled_samples = 3;                        // InMotion=false 连续稳定采样数
};
```

**自定义等待选项：**
```cpp
Codroid::MotionWaitOptions wait;
wait.timeout_s = 30.0;           // 30 秒超时

robot.MovLSync(target, 150, 500, wait);
```

### 返回值

- `true` — 到达目标
- `false` — 超时未到达

---

## 运动控制

### PauseRobotMotion / ResumeRobotMotion / StopRobotMove

```cpp
CommandResult PauseRobotMotion(int id = 1);
CommandResult ResumeRobotMotion(int id = 1);
CommandResult StopRobotMove(int id = 1);
```

暂停、恢复、停止当前运动。

---

## MoveTo（规划运动）

### MoveToParams

```cpp
struct MoveToParams {
    MoveToType type = MoveToType::Home;
    MoveToTarget target;

    MoveToParams() = default;
    explicit MoveToParams(MoveToType t);
    MoveToParams(MoveToType t, const MoveToTarget& tgt);
};
```

### MoveToType

```cpp
enum class MoveToType : int {
    Stop = -1,       // 停止 MoveTo
    Home = 0,        // 回 Home
    Safe = 1,        // 回安全位
    Candle = 2,      // 回 Candle 位
    Packing = 3,     // 回 Packing 位
    Joint = 4,       // 关节规划到目标
    Line = 5,        // 直线规划到目标
    ResumePoint = 6
};
```

### MoveToTarget

```cpp
struct MoveToTarget {
    std::vector<double> cp;
    std::vector<double> jp;

    static MoveToTarget Joint(JointPoint joint);
    static MoveToTarget Cartesian(CartesianPoint cart);
};
```

### 使用示例

```cpp
// 回 Home
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Home));

// 关节规划到目标
auto target = Codroid::MoveToTarget::Joint(
    Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}));
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Joint, target));

// 心跳（每 ≥500ms 调用）
while (moving) {
    robot.MoveToHeartbeat();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}

// 停止
robot.StopMoveTo();
```

---

## Jog（点动）

### JogParams

```cpp
struct JogParams {
    JogMode mode = JogMode::Line;
    double speed = 0.0;     // -1~1 的比例
    int index = 1;          // 轴号或方向
    CoorType coorType = CoorType::User; // Jog 协议下发 User=0, Tool=1
    int coorId = 1;

    JogParams() = default;
    JogParams(JogMode m, double s, int i, CoorType ct = CoorType::User, int cid = 1);
};
```

### JogMode

```cpp
enum class JogMode {
    Joint = 1,  // 关节点动
    Line = 2    // 直线点动
};
```

### 使用示例

```cpp
// 关节 1 正向点动，速度 50%
robot.Jog(Codroid::JogParams(Codroid::JogMode::Joint, 0.5, 1));

// 笛卡尔 X 正向点动，速度 30%
robot.Jog(Codroid::JogParams(Codroid::JogMode::Line, 0.3, 1));

// 用户坐标系 2 下的点动
robot.Jog(Codroid::JogParams(Codroid::JogMode::Line, 0.3, 1, Codroid::CoorType::User, 2));

// 心跳
while (jogging) {
    robot.JogHeartbeat();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}

// 停止
robot.StopJog();
```

---

## 最佳实践

### 1. 始终使用 MmDegWithRef

```cpp
// 好：指定参考关节
auto state = robot.GetRobotRealtimeState();
auto target = Codroid::CartesianPoint::MmDegWithRef(
    {400, 0, 300, 180, 0, 0},
    state.joint_position
);

// 不推荐：不指定参考关节
auto target = Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0});
```

### 2. 使用阻塞运动确保到达

```cpp
// 非阻塞：立即返回，不等待到达
robot.MovL(target, 150, 500);

// 阻塞：等待到达目标
robot.MovLSync(target, 150, 500);
```

### 3. 合理设置过渡半径

```cpp
// 精确停止（适合最终定位）
auto inst1 = Codroid::ClientMoveInstruction::MovL(target1, 150, 500, 0);

// 过渡（适合连续路径，提高效率）
auto inst2 = Codroid::ClientMoveInstruction::MovL(target2, 150, 500, 50);
```

### 4. 路径段间避免重复首点

```cpp
// 好：路径段连续
std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(jp1, 40, 100),
    Codroid::ClientMoveInstruction::MovL(cp2, 150, 500),  // 从 jp1 终点到 cp2
    Codroid::ClientMoveInstruction::MovL(cp3, 150, 500)   // 从 cp2 终点到 cp3
};
```
