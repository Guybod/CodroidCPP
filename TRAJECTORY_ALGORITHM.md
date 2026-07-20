# TRAJECTORY_ALGORITHM.md — 离线轨迹生成算法（C# / Python / C++ 共用）

本文档把 **C# `CodroidSDK/Trajectory.cs`** 中的算法抽出来，作为 **Python、C++** SDK 实现等价 `TrajectoryGenerator` 的算法基线。**任何语言实现都应在本文规定的输入下产生数值上完全一致的输出**（受限于浮点精度），便于跨语言离线对照与回归测试。

> 跨语言契约（端口、单位、状态位、CRI 工作流等）见 **`AGENTS.md`**。
> C# 类设计与 API 形态见 **`SDK_API_AND_DESIGN.md` §7**。

---

## 0. 约定与符号

| 符号 | 含义 |
|------|------|
| `q0`, `qf` | 关节空间起 / 终点（6 维，单位 **deg**） |
| `p0`, `pf` | 笛卡尔空间起 / 终点（6 维 `[x,y,z,rx,ry,rz]`，单位 **mm + deg**） |
| `D` | 标量距离：关节为「位移最大轴」绝对值；笛卡尔为线位移 `‖Δxyz‖₂` |
| `T` | 段总时长（秒） |
| `s(t) ∈ [0, 1]` | 时间标度（normalized arc-length），单调非减，`s(0)=0, s(T)=1` |
| `dt = 1 / FrequencyHz` | 采样步长 |
| `n = max(2, ceil(T / dt) + 1)` | 采样点数（保证至少首末两点） |

**坐标系约定（笛卡尔）**：右手坐标系，姿态采用 **固定欧拉角 XYZ 外旋**：

```
R(rx, ry, rz) = Rz(rz) · Ry(ry) · Rx(rx)
```

对应四元数：`q = qz · qy · qx`（`qa = (cos(a/2), axis · sin(a/2))`，与 Codroid TCP 位姿语义一致）。

**采样点输出**：每个采样点为 `{ t: 秒, position: double[6] }`；首点 `t = 0`、末点 `t = T`（最后一点的 `t` 通过 `min(k·dt, T)` 截断到 `T`）。

---

## 1. 整体流程

```
Generate(start, target, request):
  ValidateInputs(start, target, request)
  if request.Space == Joint:       return GenerateJoint(start, target, request)
  if request.Space == Cartesian:   return GenerateCartesian(start, target, request)
```

### 1.1 输入校验（`ValidateInputs`）

下列任何一项不满足都立即抛参数异常（实现细节按各语言习惯）：

1. `start` / `target` 非空且长度 == 6
2. `request.FrequencyHz > 0`
3. `request.Speed.HasValue XOR request.DurationSeconds.HasValue`（**二选一且只能一个**）
4. `Speed > 0`（若给）；`DurationSeconds > 0`（若给）
5. `request.Acceleration > 0`

---

## 2. 时间标度 `IMotionProfile`

抽象接口（伪代码）：

```
interface IMotionProfile {
  double T               // 段总时长
  double ScaleAt(double t)  // 返回 s(t) ∈ [0, 1]，t ∈ [0, T]
}
```

实现两种：`CubicProfile`、`TrapezoidalProfile`。

### 2.1 `CubicProfile`（三次多项式时间标度）

输入：距离 `D`，请求 `request`。

```
T:
  if request.DurationSeconds is set: T = request.DurationSeconds
  else:                              T = D / request.Speed     # 平均速度推导
```

**形状（hermite，s 形）**：

```
ScaleAt(t):
  if t <= 0: return 0
  if t >= T: return 1
  τ = t / T
  return 3·τ² − 2·τ³
```

性质：`s(0)=0, s(T)=1, s'(0)=s'(T)=0`；峰值斜率在 `τ=0.5` 处约为 `1.5`，对应**峰值速度** ≈ `1.5 · D / T`，平均速度 = `D / T`。**起止速度为 0，平滑无冲击；但不能保证严格匀速段**。

### 2.2 `TrapezoidalProfile`（梯形速度规划）

形状：加速段 `[0, ta]` → 匀速段 `[ta, T-ta]` → 减速段 `[T-ta, T]`，输出标度 `s(t) = arc(t) / D`。`ScaleAt` 实现：

```
ScaleAt(t):
  if t <= 0: return 0
  if t >= T: return 1
  if t < Ta:                 arc = 0.5 · A · t²
  else if t > T − Ta:        tt = T − t
                             arc = D − 0.5 · A · tt²
  else:                      arc = 0.5 · A · Ta² + V · (t − Ta)
  return clamp(arc / D, 0, 1)
```

参数 `(T, D, V, A, Ta)` 的求法分两个分支：

#### 2.2.1 Speed 模式 `FromSpeed(D, v, a)`

```
ta = v / a
da = 0.5 · v · ta            # 加速段位移
if 2·da >= D:
  # 距离不足以达到匀速段 → 退化为三角形
  vp = sqrt(a · D)
  ta = vp / a
  return Trapezoid(T = 2·ta, D = D, V = vp, A = a, Ta = ta)
tc = (D − 2·da) / v          # 匀速段时长
return Trapezoid(T = 2·ta + tc, D = D, V = v, A = a, Ta = ta)
```

#### 2.2.2 Duration 模式 `FromDuration(D, T, a)`

由 `D = v · (T − ta), ta = v / a` 推得二次方程 `v² − a·T·v + a·D = 0`：

```
disc = a²·T² − 4·a·D
if disc < 0:
  # 给定 (T, a) 无法走完 D → 退化为对称三角形 + 等效加速度
  vp   = 2·D / T
  aEff = 4·D / T²
  return Trapezoid(T = T, D = D, V = vp, A = aEff, Ta = T / 2)
v  = (a·T − sqrt(disc)) / 2     # 取小根：满足 v ≤ a·T/2 的物理解
ta = v / a
return Trapezoid(T = T, D = D, V = v, A = a, Ta = ta)
```

> **为什么取小根**：`v² − a·T·v + a·D = 0` 两根分别对应「先加速到 v，匀速一段，减速到 0」（小根，含匀速段，物理可行）与「无匀速段、纯三角形对称」（大根，对应 `v = a·T/2`）。需要尽量长的匀速段时取小根。

---

## 3. 关节空间 `GenerateJoint(q0, qf, request)`

```
maxDelta = max_i |qf[i] − q0[i]|
if maxDelta < 1e-9:
  yield { t: 0, position: copy(q0) }
  return                    # 起止重合：单点退化

profile = ComputeProfile(maxDelta, request)
dt = 1 / request.FrequencyHz
n  = max(2, ceil(profile.T / dt) + 1)

for k in 0..n-1:
  t = min(k · dt, profile.T)
  s = profile.ScaleAt(t)
  pos[i] = q0[i] + s · (qf[i] − q0[i])  for i in 0..5
  yield { t: t, position: pos }
```

**含义**：用「位移最大轴」决定段时长，所有六轴共享同一 `s(t)`，故 **同时启动、同时到达**；其它轴按比例缩放，没有任何轴提前到位。

---

## 4. 笛卡尔空间 `GenerateCartesian(p0, pf, request)`

```
dx = pf[0] − p0[0]
dy = pf[1] − p0[1]
dz = pf[2] − p0[2]
D  = sqrt(dx² + dy² + dz²)

# 选择「时间标度的距离」
if D >= 1e-9:
  profile = ComputeProfile(D, request)         # 用线位移 → 笛卡尔线速度可控
else:
  # 纯姿态运动（线位移可忽略）
  if request.Speed is set:
    raise ArgumentException("纯姿态运动请改用 DurationSeconds")
  profile = ComputeProfile(1.0, request)       # 归一化距离 1.0 + Duration

q0_quat = EulerXyz.ToQuaternion(p0[3], p0[4], p0[5])
qf_quat = EulerXyz.ToQuaternion(pf[3], pf[4], pf[5])

dt = 1 / request.FrequencyHz
n  = max(2, ceil(profile.T / dt) + 1)

for k in 0..n-1:
  t = min(k · dt, profile.T)
  s = profile.ScaleAt(t)

  x = p0[0] + s · dx                            # 直线插值
  y = p0[1] + s · dy
  z = p0[2] + s · dz

  q       = EulerXyz.Slerp(q0_quat, qf_quat, s)
  rx,ry,rz = EulerXyz.FromQuaternion(q)

  yield { t: t, position: [x, y, z, rx, ry, rz] }
```

**关键点**：

- 位置直线 + 姿态 SLERP 共享同一 `s(t)`，故梯形匀速段下 **TCP 线速度恒定，角速度按比例同步**；姿态变化在整段内均匀分摊，不会出现"先转后走"或"先走后转"。
- **纯姿态**分支：用线速度推导时间无定义，强制走 Duration 模式；否则抛错。

---

## 5. Euler XYZ ↔ 四元数 ↔ SLERP

**所有内部计算用双精度**，避免 32 位浮点累计误差。

### 5.1 `ToQuaternion(rxDeg, ryDeg, rzDeg) → q`

```
a = rxDeg · π/180 / 2
b = ryDeg · π/180 / 2
c = rzDeg · π/180 / 2

cx = cos(a); sx = sin(a)
cy = cos(b); sy = sin(b)
cz = cos(c); sz = sin(c)

# q = qz * qy * qx 展开
w = cz·cy·cx + sz·sy·sx
x = cz·cy·sx − sz·sy·cx
y = cz·sy·cx + sz·cy·sx
z = sz·cy·cx − cz·sy·sx
```

### 5.2 `FromQuaternion(q) → (rxDeg, ryDeg, rzDeg)`

由 `R = Rz·Ry·Rx`，从旋转矩阵元素反推：`R[2][0] = −sin(β) = 2·(x·z − w·y)`，所以 `sin(β) = 2·(w·y − x·z)`。

```
sb = clamp(2·(w·y − x·z), −1, 1)
ry = asin(sb)

if |sb| < 0.999999:
  rx = atan2(2·(y·z + w·x), 1 − 2·(x² + y²))
  rz = atan2(2·(x·y + w·z), 1 − 2·(y² + z²))
else:
  # 万向锁附近：固定 rz = 0，由 R 中其它元素求 rx
  rx = atan2(−2·(y·z − w·x), 1 − 2·(x² + z²))
  rz = 0

return (rx, ry, rz)  转回 deg
```

> **为何在 `|sin β| > 0.999999` 处退化**：当 `cos β → 0` 时常规公式分母趋零，rx / rz 数值爆炸；**此时 rx 与 rz 不可分**（仅 `rx ± rz` 可定），固定 `rz = 0` 把全部"剩余角度"分配给 `rx`，避免 NaN。SLERP 已保证四元数连续，分量不会真的卡在病态点上。

### 5.3 `Slerp(q0, q1, t) → q`

```
dot = q0·q1                              # 四维点积

# 走 SO(3) 上的最短路径（共轭半球修正）
if dot < 0:
  q1  = -q1                              # 取反不改变所表示的旋转
  dot = -dot

const LerpThreshold = 0.9995
if dot > LerpThreshold:
  # q0、q1 几乎同向，sin(θ) → 0 会除零，退化为归一化线性插值
  q = (1 − t)·q0 + t·q1
  return normalize(q)

θ0 = acos(dot)                           # 总夹角
θ  = θ0 · t                              # 当前夹角

s0 = cos(θ) − dot · sin(θ) / sin(θ0)
s1 = sin(θ) / sin(θ0)

q  = s0·q0 + s1·q1                       # 已自然归一化
```

> **`dot < 0` 修正**为何必需：四元数 `q` 与 `−q` 表示同一旋转，但 SLERP 直接用会沿"长边"走（最大 360°）。取反让夹角 ≤ 90°，走短边。这一步直接对应"欧拉数值跨过 ±180° / ±360° 时不绕远路"。
> **`dot > 0.9995` 退化**避免 `sin(θ0) → 0` 数值除零；此时线性插值与 SLERP 数值差小于 1e-6，且 `normalize` 抹平方向偏差。

---

## 6. 多段拼接（参考 `CodroidCRITest/Program.cs::GenerateMultiSegment`）

输入：路点列表 `waypoints[0..m]`（`m+1` 个 6 维点）+ `request`。
输出：合并的 `(t, pos)` 序列。

```
result   = []
tBase    = 0
for i in 0..m-1:
  seg = TrajectoryGenerator.Generate(waypoints[i], waypoints[i+1], request)
  for k in 0..seg.length-1:
    if i > 0 and k == 0:
      continue                  # 跳过后续段首点，避免与前段末点重复
    result.add({ t: tBase + seg[k].t, pos: seg[k].pos })
  tBase += seg.last.t
return result
```

**为何端点对齐 OK**：`Cubic` 与 `Trapezoidal` 在端点处 `s'=0`（速度 0），段间直接首尾相连，不需要插停顿；不同段的 `Profile`、`Speed`、`Acceleration` 可以不同（典型场景：先匀速直线，再 Cubic 平滑摆姿态）。

**注意**：相邻段的"端点重复"必须显式去重，否则下发到控制器的 UDP 帧时间戳会错位 / 出现重复坐标。

---

## 7. 边界条件与错误情形清单

| 情形 | 行为 |
|------|------|
| `start == target`（关节，maxDelta < 1e-9） | 单点序列 `{ t=0, pos=q0 }` |
| `start == target`（笛卡尔，D < 1e-9，**且姿态也相同**） | 退化为单点（`Slerp` 在等姿态下取极值即原四元数） |
| `start == target`（笛卡尔，D < 1e-9，**仅纯姿态变**） + Speed | 抛 ArgumentException |
| `start == target`（笛卡尔，D < 1e-9，**仅纯姿态变**） + Duration | 用归一化距离 1.0 求 profile，正常生成 |
| 关节最大轴 `Δq` 与其它轴量级悬殊 | 由 `s(t)` 同一时间标度同步，所有轴同时启停（不会某轴先到） |
| `Trapezoidal.FromSpeed` 中 `2·da ≥ D` | 退化三角形：`vp = √(a·D)`, `ta = vp/a`, `T = 2·ta` |
| `Trapezoidal.FromDuration` 中 `disc < 0` | 退化对称三角形 + 等效加速度 `aEff = 4·D/T²` |
| `Slerp` 中 `dot < 0` | 取 `q1 = -q1`，走最短半球 |
| `Slerp` 中 `dot > 0.9995` | 退化为 nlerp（线性插值 + 归一化） |
| `FromQuaternion` 中 `\|sin β\| > 0.999999`（万向锁） | 固定 `rz = 0` 求 `rx` |

---

## 8. 跨语言落地清单

实现新语言版本时，按下表打勾：

- [ ] **类型定义**：`TrajectorySpace`、`TrajectoryProfile`、`TrajectoryRequest`、`TrajectoryPoint` 字段名、单位与 C# 一致。
- [ ] **入参校验**：与 §1.1 完全一致，错误类型用各语言惯用异常（Python `ValueError` / C++ `std::invalid_argument` 等）。
- [ ] **`CubicProfile.ScaleAt`** 与 §2.1 表达式逐字一致。
- [ ] **`TrapezoidalProfile.From{Speed,Duration}`** 含两个退化分支（§2.2.1 / §2.2.2）。
- [ ] **`GenerateJoint`** 返回采样点数 `n = max(2, ceil(T/dt)+1)`，最后一点 `t = T`。
- [ ] **`GenerateCartesian`** 直线 + SLERP + 时间标度共享；纯姿态 + Speed 抛错。
- [ ] **`EulerXyz`** 使用双精度，约定 `q = qz·qy·qx`，`FromQuaternion` 含万向锁分支。
- [ ] **`Slerp`** 含 `dot<0` 共轭与 `dot>0.9995` 线性退化。
- [ ] **多段拼接**跳过后续段首点。
- [ ] **数值回归**：用 §9 给定测试向量跑一次，与 C# 输出在小数点后 6 位一致。

---

## 9. 数值回归测试向量（C# / Python / C++ 必须一致）

每条用例标注 **输入** 与 **输出关键采样点**（不是逐点全列；语言间允许 1e-6 量级浮点误差）。

> 跑参考实现时调用：
> ```csharp
> var pts = TrajectoryGenerator.Generate(start, target, request).ToList();
> // 验收：pts[0].t == 0, pts[^1].t == request 算出的 T，长度 == n
> ```

### 9.1 Joint · Cubic · Speed

| 字段 | 值 |
|------|-----|
| `start` | `[0, 0, 0, 0, 0, 0]` |
| `target` | `[0, 0, 90, 0, 90, 0]` |
| `Space` | Joint |
| `Profile` | Cubic |
| `Speed` | 30 deg/s |
| `FrequencyHz` | 250 |

预期：`maxDelta = 90`，`T = 90 / 30 = 3.0 s`，`n = max(2, ceil(3.0·250)+1) = 751`。

| 索引 | t (s) | 期望位置 |
|------|-------|----------|
| 0 | 0.000 | `[0, 0, 0, 0, 0, 0]` |
| 375 | 1.500 | `s = 0.5` → `[0, 0, 45, 0, 45, 0]` |
| 750 | 3.000 | `[0, 0, 90, 0, 90, 0]` |

### 9.2 Cartesian · Trapezoidal · Speed（等姿态）

| 字段 | 值 |
|------|-----|
| `start` | `[1000, 0, 500, 180, 0, -90]` |
| `target` | `[1000, 0, 300, 180, 0, -90]`（z 下移 200 mm） |
| `Profile` | Trapezoidal |
| `Speed` | 80 mm/s |
| `Acceleration` | 400 mm/s² |
| `FrequencyHz` | 250 |

预期：`D = 200`，`ta = 80/400 = 0.2 s`，`da = 0.5·80·0.2 = 8`，`2·da = 16 < 200` → 含匀速段。`tc = (200 − 16) / 80 = 2.3 s`，`T = 2·0.2 + 2.3 = 2.7 s`。

| 索引 | t (s) | 期望 `[x,y,z,rx,ry,rz]` |
|------|-------|--------------------------|
| 0 | 0.000 | `[1000, 0, 500, 180, 0, -90]` |
| 任意中段 t ∈ [0.2, 2.5] | — | z 减小斜率为 −80 mm/s（匀速段恒定） |
| 末点 | 2.700 | `[1000, 0, 300, 180, 0, -90]` |

姿态在等姿态情况下整段保持不变（`Slerp(q,q,s) = q`）。

### 9.3 Cartesian · Trapezoidal · Speed（含姿态变化）

| 字段 | 值 |
|------|-----|
| `start` | `[1139.994, -222.730, 899.022, -91.506, -0.002, -136.466]` |
| `target` | `[915.480, -73.000, 599.316, 166.910, -5.170, -90.726]` |
| `Profile` | Trapezoidal |
| `Speed` | 80 mm/s |
| `Acceleration` | 400 mm/s² |
| `FrequencyHz` | 250 |

预期：`D ≈ 397.7 mm`（自行计算），姿态用四元数 SLERP；P2→P3 的欧拉数值 `rx` 跨过 `±180°` 边界，**SLERP 走 SO(3) 测地线**实际旋转 ~104°（不到一半），不会绕远路。

数值断言：`pts[0].pos ≈ start`，`pts[last].pos ≈ target`（小数点后 6 位）；中间点的姿态四元数夹角随 `s` 线性变化（`acos(dot(q0, q(s))) = s · acos(dot(q0, qf))`）。

### 9.4 Cartesian · Cubic · Duration · 纯姿态

| 字段 | 值 |
|------|-----|
| `start` | `[500, 0, 500, 0, 0, 0]` |
| `target` | `[500, 0, 500, 30, 0, 0]`（仅 rx 转 30°） |
| `Profile` | Cubic |
| `DurationSeconds` | 2.0 |
| `FrequencyHz` | 250 |

预期：`D < 1e-9` 走纯姿态分支，`profile.T = 2.0`，`n = 501`。
末点 `pts[^1].position` 应为 `[500, 0, 500, 30, 0, 0]`，中点 `t = 1.0`、`s = 0.5`，rx ≈ 15°（线性插值四元数球面距离的一半 = 欧拉 rx 的一半，**仅当单轴旋转时**）。

---

## 10. 与 SDK 其它部分的关系

- **API / 类形态** → `SDK_API_AND_DESIGN.md` §7.1
- **CRI 实时控制工作流（StartControl 时序、CommandData 单位与字节布局）** → `AGENTS.md` §6 + `SDK_API_AND_DESIGN.md` §7.2
- **测试输入数据（CodroidCRITest 三段实测点）** → `AGENTS.md` §5.2

如果未来调整算法（如改用 5 次多项式、加 jerk 约束、改 SLERP 为 squad），**先改本文档与 C# 实现，再同步 Python / C++**，并在 §9 增补对应回归向量。
